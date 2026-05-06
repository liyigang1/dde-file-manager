// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "textindexcreatorservice.h"
#include "config.h"

#include <controllerpipe.h>

#include <QTimer>

SERVICETEXTINDEX_BEGIN_NAMESPACE

// Timeout constants
namespace {
constexpr int kExtractorIdleShutdownMs = 60000;
}

/**
 * @brief Internal state tracking for pending requests
 */
struct PendingRequest
{
    QString requestId;
    enum Type {
        CreateIndex,         // Create index operation
        UpdateIndex,         // Update index operation
        ProcessFileList,     // Process file list (create/update/remove)
        ProcessFileMoves     // Process file moves
    } type;

    // For index operations
    qint64 processedCount = 0;
    qint64 totalCount = 0;
};

class TextIndexCreatorService::TextIndexCreatorServicePrivate
{
public:
    QString extractorPath;
    TEXTINDEX_CREATOR_IPC_NAMESPACE::ControllerPipe *pipe = nullptr;
    QTimer idleShutdownTimer;

    // Current active request (only one operation at a time)
    QScopedPointer<PendingRequest> currentRequest;

    // Flag to prevent re-entrant stopPipe calls
    bool stoppingPipe = false;
};

TextIndexCreatorService::TextIndexCreatorService(QObject *parent)
    : QObject(parent)
    , d(new TextIndexCreatorServicePrivate())
{
    d->extractorPath = QStringLiteral(DFM_EXTRACTOR_TOOL);
    d->pipe = new TEXTINDEX_CREATOR_IPC_NAMESPACE::ControllerPipe(this);
    d->idleShutdownTimer.setSingleShot(true);

    setupConnections();
}

TextIndexCreatorService::~TextIndexCreatorService()
{
    shutdown();
}

bool TextIndexCreatorService::initialize()
{
    if (d->pipe->isRunning()) {
        return true;
    }

    if (d->extractorPath.isEmpty()) {
        fmCritical() << "TextIndexCreatorService: Extractor path is empty";
        return false;
    }

    if (!d->pipe->start(d->extractorPath)) {
        fmCritical() << "TextIndexCreatorService: Failed to start extractor process";
        return false;
    }

    fmInfo() << "TextIndexCreatorService: Extractor process started, pid:" << d->pipe->processId();
    return true;
}

void TextIndexCreatorService::shutdown()
{
    d->idleShutdownTimer.stop();

    // Cancel any pending request
    if (d->currentRequest) {
        IndexOperationResult result;
        result.success = false;
        result.errorMessage = "Service shutting down";
        emit indexOperationFinished(d->currentRequest->requestId, result);
        d->currentRequest.reset();
    }

    if (d->pipe && d->pipe->isRunning()) {
        fmInfo() << "TextIndexCreatorService: Stopping extractor process";
        d->stoppingPipe = true;
        d->pipe->stop();
        d->stoppingPipe = false;
    }
}

bool TextIndexCreatorService::isRunning() const
{
    return d->pipe && d->pipe->isRunning();
}

QString TextIndexCreatorService::createIndex(const QString &sourcePath, bool silent)
{
    if (d->currentRequest) {
        fmWarning() << "TextIndexCreatorService: Busy with another request, cannot create index";
        return QString();
    }

    d->idleShutdownTimer.stop();

    if (!isRunning() && !initialize()) {
        fmCritical() << "TextIndexCreatorService: Failed to start extractor for create index";
        return QString();
    }

    d->currentRequest.reset(new PendingRequest());
    d->currentRequest->requestId = generateRequestId();
    d->currentRequest->type = PendingRequest::CreateIndex;

    fmInfo() << "TextIndexCreatorService: Starting create index, requestId:" << d->currentRequest->requestId
             << "source:" << sourcePath << "silent:" << silent;

    if (!d->pipe->createIndex(sourcePath, silent)) {
        fmWarning() << "TextIndexCreatorService: Failed to send create index request";
        d->currentRequest.reset();
        return QString();
    }

    return d->currentRequest->requestId;
}

QString TextIndexCreatorService::updateIndex(const QString &sourcePath, bool silent)
{
    if (d->currentRequest) {
        fmWarning() << "TextIndexCreatorService: Busy with another request, cannot update index";
        return QString();
    }

    d->idleShutdownTimer.stop();

    if (!isRunning() && !initialize()) {
        fmCritical() << "TextIndexCreatorService: Failed to start extractor for update index";
        return QString();
    }

    d->currentRequest.reset(new PendingRequest());
    d->currentRequest->requestId = generateRequestId();
    d->currentRequest->type = PendingRequest::UpdateIndex;

    fmInfo() << "TextIndexCreatorService: Starting update index, requestId:" << d->currentRequest->requestId
             << "source:" << sourcePath << "silent:" << silent;

    if (!d->pipe->updateIndex(sourcePath, silent)) {
        fmWarning() << "TextIndexCreatorService: Failed to send update index request";
        d->currentRequest.reset();
        return QString();
    }

    return d->currentRequest->requestId;
}

QString TextIndexCreatorService::processFileList(const QStringList &fileList,
                                                 TEXTINDEX_CREATOR_IPC_NAMESPACE::FileListOperationType operation, bool silent)
{
    if (d->currentRequest) {
        fmWarning() << "TextIndexCreatorService: Busy with another request, cannot process file list";
        return QString();
    }

    if (fileList.isEmpty()) {
        return QString();
    }

    d->idleShutdownTimer.stop();

    if (!isRunning() && !initialize()) {
        fmCritical() << "TextIndexCreatorService: Failed to start extractor for file list";
        return QString();
    }

    d->currentRequest.reset(new PendingRequest());
    d->currentRequest->requestId = generateRequestId();
    d->currentRequest->type = PendingRequest::ProcessFileList;

    fmInfo() << "TextIndexCreatorService: Starting process file list, requestId:" << d->currentRequest->requestId
             << "files:" << fileList.size() << "operation:" << static_cast<int>(operation)
             << "silent:" << silent;

    if (!d->pipe->processFileList(fileList, operation, silent)) {
        fmWarning() << "TextIndexCreatorService: Failed to send process file list request";
        d->currentRequest.reset();
        return QString();
    }

    return d->currentRequest->requestId;
}

QString TextIndexCreatorService::processFileMoves(const QHash<QString, QString> &movedFiles, bool silent)
{
    if (d->currentRequest) {
        fmWarning() << "TextIndexCreatorService: Busy with another request, cannot process file moves";
        return QString();
    }

    if (movedFiles.isEmpty()) {
        return QString();
    }

    d->idleShutdownTimer.stop();

    if (!isRunning() && !initialize()) {
        fmCritical() << "TextIndexCreatorService: Failed to start extractor for file moves";
        return QString();
    }

    d->currentRequest.reset(new PendingRequest());
    d->currentRequest->requestId = generateRequestId();
    d->currentRequest->type = PendingRequest::ProcessFileMoves;

    fmInfo() << "TextIndexCreatorService: Starting process file moves, requestId:" << d->currentRequest->requestId
             << "moves:" << movedFiles.size() << "silent:" << silent;

    if (!d->pipe->processFileMoves(movedFiles, silent)) {
        fmWarning() << "TextIndexCreatorService: Failed to send process file moves request";
        d->currentRequest.reset();
        return QString();
    }

    return d->currentRequest->requestId;
}

void TextIndexCreatorService::stopCurrentTask()
{
    if (!isRunning()) {
        return;
    }

    fmInfo() << "TextIndexCreatorService: Sending stop task command";
    d->pipe->stopTask();
}

QString TextIndexCreatorService::generateRequestId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void TextIndexCreatorService::setupConnections()
{
    // Connect task finished signal (for index operations)
    connect(d->pipe, &TEXTINDEX_CREATOR_IPC_NAMESPACE::ControllerPipe::taskFinished,
            this, &TextIndexCreatorService::handleTaskFinished);

    // Connect progress signal
    connect(d->pipe, &TEXTINDEX_CREATOR_IPC_NAMESPACE::ControllerPipe::progressChanged,
            this, &TextIndexCreatorService::handleProgressChanged);

    // Connect process crashed signal
    connect(d->pipe, &TEXTINDEX_CREATOR_IPC_NAMESPACE::ControllerPipe::processCrashed,
            this, &TextIndexCreatorService::handleProcessCrashed);

    // Connect error occurred signal
    connect(d->pipe, &TEXTINDEX_CREATOR_IPC_NAMESPACE::ControllerPipe::errorOccurred,
            this, &TextIndexCreatorService::handleErrorOccurred);

    // Connect process finished signal
    connect(d->pipe, &TEXTINDEX_CREATOR_IPC_NAMESPACE::ControllerPipe::processFinished,
            this, &TextIndexCreatorService::handleProcessFinished);

    // Connect idle shutdown timer
    connect(&d->idleShutdownTimer, &QTimer::timeout, this, [this]() {
        if (!d->currentRequest && isRunning()) {
            fmInfo() << "TextIndexCreatorService: Stopping idle extractor process after timeout";
            d->stoppingPipe = true;
            d->pipe->stop();
            d->stoppingPipe = false;
        }
    });
}

void TextIndexCreatorService::handleTaskFinished(bool success, qint64 processedCount, const QString &errorMessage,
                                                  bool interrupted, bool useAnything, bool fatal)
{
    if (!d->currentRequest) {
        return;
    }

    IndexOperationResult result;
    result.success = success;
    result.interrupted = interrupted;
    result.useAnything = useAnything;
    result.fatal = fatal;
    result.processedCount = processedCount;
    result.errorMessage = errorMessage;

    fmInfo() << "TextIndexCreatorService: Task finished, requestId:" << d->currentRequest->requestId
             << "success:" << success << "interrupted:" << interrupted
             << "useAnything:" << useAnything << "fatal:" << fatal
             << "processed:" << processedCount;

    emit indexOperationFinished(d->currentRequest->requestId, result);
    d->currentRequest.reset();

    if (isRunning()) {
        d->idleShutdownTimer.start(kExtractorIdleShutdownMs);
    }
}

void TextIndexCreatorService::handleProgressChanged(qint64 processedCount, qint64 totalCount, const QString &currentFile)
{
    if (!d->currentRequest) {
        return;
    }

    d->currentRequest->processedCount = processedCount;
    d->currentRequest->totalCount = totalCount;

    OperationProgress progress;
    progress.processedCount = processedCount;
    progress.totalCount = totalCount;
    progress.currentFile = currentFile;

    emit progressReported(d->currentRequest->requestId, progress);
}

void TextIndexCreatorService::handleProcessCrashed()
{
    fmWarning() << "TextIndexCreatorService: Extractor process crashed";

    // Cancel any pending request
    if (d->currentRequest) {
        IndexOperationResult result;
        result.success = false;
        result.errorMessage = "Extractor process crashed";
        emit indexOperationFinished(d->currentRequest->requestId, result);
        d->currentRequest.reset();
    }

    emit processCrashed();
}

void TextIndexCreatorService::handleErrorOccurred(const QString &error)
{
    if (d->stoppingPipe) {
        return;
    }

    fmWarning() << "TextIndexCreatorService: Error occurred:" << error;

    // If there's an active request, fail it
    if (d->currentRequest) {
        IndexOperationResult result;
        result.success = false;
        result.errorMessage = error;
        emit indexOperationFinished(d->currentRequest->requestId, result);
        d->currentRequest.reset();
    }

    emit errorOccurred(error);
}

void TextIndexCreatorService::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (d->stoppingPipe) {
        return;
    }

    if (exitStatus == QProcess::CrashExit) {
        return;   // Already handled by processCrashed
    }

    fmWarning() << "TextIndexCreatorService: Extractor process finished unexpectedly with exit code:" << exitCode;

    // Cancel any pending request
    if (d->currentRequest) {
        IndexOperationResult result;
        result.success = false;
        result.errorMessage = "Extractor process finished unexpectedly";
        emit indexOperationFinished(d->currentRequest->requestId, result);
        d->currentRequest.reset();
    }
}

SERVICETEXTINDEX_END_NAMESPACE
