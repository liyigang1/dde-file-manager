// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "indextask.h"

#include "utils/systemdcpuutils.h"
#include "utils/textindexconfig.h"

#include <LuceneException.h>

#include <QDebug>
#include <QThread>
#include <QCoreApplication>

SERVICETEXTINDEX_USE_NAMESPACE

IndexTask::IndexTask(Type type, const QString &path, TextIndexCreatorService *service,
                     const QStringList &fileList, const QHash<QString, QString> moveFiles, QObject *parent)
    : QObject(parent), m_type(type), m_path(path), m_currentFileList(fileList),
      m_currentMovedFiles(moveFiles), m_extractorService(service)
{
    fmInfo() << "[IndexTask] Created new task - type:" << static_cast<int>(type) << "path:" << path;

    if (m_extractorService) {
        disconnect(m_extractorService, nullptr, this, nullptr);
    }

    m_extractorService = service;
    fmInfo() << "[TaskManager] TextIndexCreatorService set:" << (service ? "valid" : "null");

    if (m_extractorService) {
        setupTextIndexCreatorServiceConnections();
    }
}

IndexTask::~IndexTask()
{
    fmDebug() << "[IndexTask] Destroying task for path:" << m_path;
}

void IndexTask::onProgressChanged(qint64 count, qint64 total)
{
    if (m_state.isRunning()) {
        // 避免在高频进度更新中打印过多日志，只在特定条件下打印
        static qint64 lastLoggedCount = 0;
        if (count == 0 || total == 0 || count == total || (count - lastLoggedCount) >= 1000) {
            fmDebug() << "[IndexTask::onProgressChanged] Task progress - type:" << static_cast<int>(m_type)
                      << "processed:" << count << "total:" << total;
            lastLoggedCount = count;
        }
        emit progressChanged(m_type, count, total);
    }
}

void IndexTask::setupTextIndexCreatorServiceConnections()
{
    if (!m_extractorService) {
        return;
    }

    // Connect progress reporting from TextIndexCreatorService
    connect(m_extractorService, &TextIndexCreatorService::progressReported,
            this, [this](const QString &requestId, const OperationProgress &progress) {
        if (requestId == m_currentRequestId) {
            onProgressChanged(progress.processedCount, progress.totalCount);
        }
    });

    // Connect task completion from TextIndexCreatorService
    connect(m_extractorService, &TextIndexCreatorService::indexOperationFinished,
            this, [this](const QString &requestId, const IndexOperationResult &result) {


        fmInfo() << "[TaskManager] IPC task finished - type:" << typeToString(m_type)
                 << "path:" << m_path << "success:" << result.success;

        if (requestId != m_currentRequestId) {
            return;
        }

        HandlerResult handerResult;
        handerResult.fatal = result.fatal;
        handerResult.success = result.success;
        handerResult.interrupted = result.interrupted;
        handerResult.useAnything = result.useAnything;

        m_state.stop();
        m_status = result.success ? Status::Finished : Status::Failed;

        emit finished(m_type, handerResult);

        m_currentRequestId.clear();
    });

    // Connect error handling
    connect(m_extractorService, &TextIndexCreatorService::errorOccurred,
            this, [this](const QString &error) {
        fmWarning() << "[TaskManager] TextIndexCreatorService error:" << error;
        IndexOperationResult result;
        result.success = false;
        result.errorMessage = error;
        m_error = error;
        emit m_extractorService->indexOperationFinished(m_currentRequestId, result);
    });

    fmInfo() << "[TaskManager] TextIndexCreatorService connections setup complete";
}

bool IndexTask::startTaskViaIPC()
{
    if (!m_extractorService) {
        fmWarning() << "[IndexTask::startTaskViaIPC] No TextIndexCreatorService available";
        return false;
    }

    QString requestId;
    if (m_type == IndexTask::Type::Create) {
        fmInfo() << "[IndexTask::startTaskViaIPC] Starting Create task via IPC - source:" << m_path;
        requestId = m_extractorService->createIndex(m_path, m_silent);
    } else if (m_type == IndexTask::Type::Update) {
        fmInfo() << "[IndexTask::startTaskViaIPC] Starting Update task via IPC - source:" << m_path;
        requestId = m_extractorService->updateIndex(m_path, m_silent);
    } else {
        fmWarning() << "[IndexTask::startTaskViaIPC] Unsupported task type for IPC:" << static_cast<int>(m_type);
        return false;
    }

    if (requestId.isEmpty()) {
        fmWarning() << "[TaskManager::startTaskViaIPC] Failed to start task via IPC";
        return false;
    }

    // Store task info for signal handlers
    m_currentRequestId = requestId;

    fmInfo() << "[TaskManager::startTaskViaIPC] Task started via IPC with requestId:" << requestId;
    return true;
}

bool IndexTask::startFileListTaskViaIPC(IndexTask::Type type, const QStringList &fileList, bool silent)
{
    if (!m_extractorService) {
        fmWarning() << "[TaskManager::startFileListTaskViaIPC] No TextIndexCreatorService available";
        return false;
    }

    if (fileList.isEmpty()) {
        fmWarning() << "[TaskManager::startFileListTaskViaIPC] File list is empty";
        return false;
    }

    // Map task type to FileListOperationType
    TEXTINDEX_CREATOR_IPC_NAMESPACE::FileListOperationType operation;
    switch (type) {
    case IndexTask::Type::CreateFileList:
        operation = TEXTINDEX_CREATOR_IPC_NAMESPACE::FileListOperationType::Create;
        break;
    case IndexTask::Type::UpdateFileList:
        operation = TEXTINDEX_CREATOR_IPC_NAMESPACE::FileListOperationType::Update;
        break;
    case IndexTask::Type::RemoveFileList:
        operation = TEXTINDEX_CREATOR_IPC_NAMESPACE::FileListOperationType::Remove;
        break;
    default:
        fmWarning() << "[TaskManager::startFileListTaskViaIPC] Unsupported task type:" << static_cast<int>(type);
        return false;
    }

    fmInfo() << "[TaskManager::startFileListTaskViaIPC] Starting file list task via IPC - files:" << fileList.size()
             << "operation:" << static_cast<int>(operation);

    QString requestId = m_extractorService->processFileList(fileList, operation, silent);
    if (requestId.isEmpty()) {
        fmWarning() << "[TaskManager::startFileListTaskViaIPC] Failed to start task via IPC";
        return false;
    }

    // Store task info for signal handlers
    m_currentRequestId = requestId;

    fmInfo() << "[TaskManager::startFileListTaskViaIPC] Task started via IPC with requestId:" << requestId;
    return true;
}

bool IndexTask::startFileMoveTaskViaIPC(const QHash<QString, QString> &movedFiles, bool silent)
{
    if (!m_extractorService) {
        fmWarning() << "[TaskManager::startFileMoveTaskViaIPC] No TextIndexCreatorService available";
        return false;
    }

    if (movedFiles.isEmpty()) {
        fmWarning() << "[TaskManager::startFileMoveTaskViaIPC] Moved files list is empty";
        return false;
    }

    fmInfo() << "[TaskManager::startFileMoveTaskViaIPC] Starting file move task via IPC - moves:" << movedFiles.size();

    QString requestId = m_extractorService->processFileMoves(movedFiles, silent);
    if (requestId.isEmpty()) {
        fmWarning() << "[TaskManager::startFileMoveTaskViaIPC] Failed to start task via IPC";
        return false;
    }

    // Store task info for signal handlers
    m_currentRequestId = requestId;
    fmInfo() << "[TaskManager::startFileMoveTaskViaIPC] Task started via IPC with requestId:" << requestId;
    return true;
}

QString IndexTask::typeToString(IndexTask::Type type)
{
    switch (type) {
    case IndexTask::Type::Create:
        return "create";
    case IndexTask::Type::Update:
        return "update";
    case IndexTask::Type::CreateFileList:
        return "create-file-list";
    case IndexTask::Type::UpdateFileList:
        return "update-file-list";
    case IndexTask::Type::RemoveFileList:
        return "remove-file-list";
    case IndexTask::Type::MoveFileList:
        return "move-file-list";
    default:
        fmWarning() << "[TaskManager::typeToString] Unknown task type:" << static_cast<int>(type);
        return "unknown";
    }
}

IndexTask::Type IndexTask::StringToType(const QString &typeStr)
{
    if (typeStr == "create") {
        return IndexTask::Type::Create;
    } else if (typeStr == "update") {
        return IndexTask::Type::Update;
    } else if (typeStr == "create-file-list") {
        return IndexTask::Type::CreateFileList;
    } else if (typeStr == "update-file-list") {
        return IndexTask::Type::UpdateFileList;
    } else if (typeStr == "remove-file-list") {
        return IndexTask::Type::RemoveFileList;
    } else if (typeStr == "move-file-list") {
        return IndexTask::Type::MoveFileList;
    } else {
        fmWarning() << "[TaskManager::StringToType] Unknown task type string:" << typeStr;
        return IndexTask::Type::Unknow;   // Default to Create
    }
}

bool IndexTask::silent() const
{
    return m_silent;
}

void IndexTask::setSilent(bool newSilent)
{
    fmDebug() << "[IndexTask::setSilent] Silent mode changed to:" << newSilent << "for path:" << m_path;
    m_silent = newSilent;
}

void IndexTask::throttleCpuUsage()
{
    if (!silent()) {
        fmDebug() << "[IndexTask::throttleCpuUsage] Skipping CPU throttling - not in silent mode";
        return;
    }

    int limit = TextIndexConfig::instance().cpuUsageLimitPercent();
    fmDebug() << "[IndexTask::throttleCpuUsage] Applying CPU usage limit:" << limit << "% for service:"
              << Defines::kTextIndexServiceName;

    QString msg;
    if (!SystemdCpuUtils::setCpuQuota(Defines::kTextIndexServiceName, limit, &msg)) {
        fmWarning() << "[IndexTask::throttleCpuUsage] Failed to set CPU quota:" << msg
                    << "service:" << Defines::kTextIndexServiceName << "limit:" << limit << "%";
    } else {
        fmDebug() << "[IndexTask::throttleCpuUsage] CPU quota applied successfully - limit:" << limit << "%";
    }
}

void IndexTask::start()
{
    if (m_state.isRunning()) {
        fmWarning() << "[IndexTask::start] Task already running, ignoring start request - path:" << m_path;
        return;
    }

    fmInfo() << "[IndexTask::start] Starting task - type:" << static_cast<int>(m_type)
             << "path:" << m_path << "silent:" << m_silent;

    m_state.start();
    m_status = Status::Running;

    Q_ASSERT(QThread::currentThread() != QCoreApplication::instance()->thread());
    fmDebug() << "[IndexTask::start] Task executing in worker thread:" << QThread::currentThread()
              << "main thread:" << QCoreApplication::instance()->thread();

    if (doTask())
        return;
    m_state.stop();
    m_status = Status::Failed;
    HandlerResult handerResult;
    handerResult.fatal = true;
    handerResult.success = false;
    handerResult.interrupted = false;
    handerResult.useAnything = false;

    emit finished(m_type, handerResult);
}

void IndexTask::stop()
{
    // Stop IPC task if running
    if (!m_currentRequestId.isEmpty() && m_extractorService) {
        fmInfo() << "[TaskManager::stopCurrentTask] Stopping IPC task - requestId:" << m_currentRequestId;
        m_extractorService->stopCurrentTask();
        return;
    }

    fmInfo() << "[IndexTask::stop] Stopping task - type:" << static_cast<int>(m_type) << "path:" << m_path;
    m_state.stop();
}

bool IndexTask::isRunning() const
{
    return m_status == Status::Running;
}

QString IndexTask::taskPath() const
{
    return m_path;
}

IndexTask::Type IndexTask::taskType() const
{
    return m_type;
}

IndexTask::Status IndexTask::status() const
{
    return m_status;
}

bool IndexTask::isIndexCorrupted() const
{
    return m_indexCorrupted;
}

void IndexTask::setIndexCorrupted(bool corrupted)
{
    if (m_indexCorrupted != corrupted) {
        fmWarning() << "[IndexTask::setIndexCorrupted] Index corruption status changed to:" << corrupted
                    << "for path:" << m_path;
        m_indexCorrupted = corrupted;
    }
}

bool IndexTask::doTask()
{
    fmInfo() << "[IndexTask::doTask] task handler - type:" << static_cast<int>(m_type)
             << "path:" << m_path;

    switch (m_type) {
    case Type::Create:
    case Type::Update:
        return startTaskViaIPC();
    case Type::Unknow:
        fmCritical() << "[IndexTask::doTask] task handler - type is unknow!";
        m_error = "index task type is unknow!";
        return false;
    case Type::MoveFileList:
        return startFileMoveTaskViaIPC(m_currentMovedFiles, m_silent);
    default:
        return startFileListTaskViaIPC(m_type, m_currentFileList, m_silent);
    }
}
