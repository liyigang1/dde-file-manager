// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "workerpipe.h"

#include <QDataStream>
#include <QIODevice>
#include <QFile>
#include <QSocketNotifier>
#include <cstdio>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

TEXTINDEX_CREATOR_IPC_BEGIN_NAMESPACE

namespace {
constexpr int kPipeReadChunkSize = 8192;
}

class WorkerPipePrivate
{
public:
    QSocketNotifier *stdinNotifier = nullptr;
    QByteArray inputBuffer;
    bool waitingForComplete = false;
    bool initialized = false;
    qint32 expectedSize = 0;
    int outputFd = -1;
};

WorkerPipe::WorkerPipe(QObject *parent)
    : QObject(parent), d(new WorkerPipePrivate())
{
}

WorkerPipe::~WorkerPipe()
{
    if (d->stdinNotifier) {
        delete d->stdinNotifier;
    }

    if (d->outputFd >= 0) {
        ::close(d->outputFd);
    }
}

bool WorkerPipe::initialize()
{
    if (d->initialized) {
        return true;
    }

    // Set stdin to binary mode
    FILE *stdinFile = freopen(nullptr, "rb", stdin);
    if (!stdinFile) {
        fmCritical() << "WorkerPipe::initialize: Failed to reopen stdin in binary mode";
        return false;
    }

    if (!setupOutputChannel()) {
        fmCritical() << "WorkerPipe::initialize: Failed to set up output channel";
        return false;
    }

    // Create socket notifier to watch stdin
    d->stdinNotifier = new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read, this);
    connect(d->stdinNotifier, &QSocketNotifier::activated,
            this, [this](int) {
        readFromStdin();
    });

    d->initialized = true;
    fmDebug() << "WorkerPipe: Initialized successfully";

    return true;
}

bool WorkerPipe::readFromStdin()
{
    QByteArray buffer(kPipeReadChunkSize, Qt::Uninitialized);
    const ssize_t bytesRead = ::read(STDIN_FILENO, buffer.data(), static_cast<size_t>(buffer.size()));

    if (bytesRead <= 0) {
        if (hasPendingPartialMessage()) {
            fmWarning() << "WorkerPipe: Stdin closed with incomplete message."
                        << "buffer size:" << d->inputBuffer.size()
                        << "expected payload size:" << d->expectedSize;
        }
        fmInfo() << "WorkerPipe: Stdin closed";
        emit stdinClosed();
        d->stdinNotifier->setEnabled(false);
        return false;
    }

    d->inputBuffer.append(buffer.constData(), static_cast<int>(bytesRead));
    processInputBuffer();
    return true;
}

void WorkerPipe::processInputBuffer()
{
    while (true) {
        if (!d->waitingForComplete) {
            if (d->inputBuffer.size() < static_cast<int>(sizeof(qint32))) {
                return;
            }

            QDataStream sizeStream(d->inputBuffer);
            sizeStream >> d->expectedSize;
            if (d->expectedSize < 0) {
                fmCritical() << "WorkerPipe: Invalid message size:" << d->expectedSize;
                d->stdinNotifier->setEnabled(false);
                emit stdinClosed();
                return;
            }

            d->waitingForComplete = true;
        }

        const qint32 totalSize = static_cast<qint32>(sizeof(qint32)) + d->expectedSize;
        const auto bufferSize = d->inputBuffer.size();
        if (bufferSize < totalSize) {
            return;
        }

        const QByteArray messageData = d->inputBuffer.mid(sizeof(qint32), d->expectedSize);
        d->inputBuffer.remove(0, totalSize);
        d->waitingForComplete = false;
        d->expectedSize = 0;

        // Parse command type
        QDataStream messageStream(messageData);
        quint8 cmdByte = 0;
        messageStream >> cmdByte;
        CommandType cmdType = static_cast<CommandType>(cmdByte);

        fmDebug() << "WorkerPipe: Received command" << commandToString(cmdType);
        emit activityDetected();

        handleCommand(cmdType, messageData);
    }
}

void WorkerPipe::handleCommand(CommandType cmdType, const QByteArray &messageData)
{
    QDataStream messageStream(messageData);

    // Skip the command type byte that was already read
    messageStream.skipRawData(1);

    switch (cmdType) {
    case CommandType::CreateIndex: {
        CreateIndexCommand cmd;
        messageStream >> cmd.sourcePath >> cmd.silent;
        fmInfo() << "WorkerPipe: CreateIndex - source:" << cmd.sourcePath
                 << "silent:" << cmd.silent;
        emit createIndexReceived(cmd);
        break;
    }

    case CommandType::UpdateIndex: {
        UpdateIndexCommand cmd;
        messageStream >> cmd.sourcePath >> cmd.silent;
        fmInfo() << "WorkerPipe: UpdateIndex - source:" << cmd.sourcePath
                 << "silent:" << cmd.silent;
        emit updateIndexReceived(cmd);
        break;
    }

    case CommandType::StopTask: {
        fmInfo() << "WorkerPipe: StopTask received";
        emit stopTaskReceived();
        break;
    }

    case CommandType::ProcessFileChanges: {
        ProcessFileChangesCommand cmd;

        qint32 changeCount = 0;
        messageStream >> changeCount;
        cmd.changes.reserve(changeCount);

        for (qint32 i = 0; i < changeCount; ++i) {
            FileChangeEntry entry;
            quint8 typeByte = 0;
            messageStream >> entry.filePath >> typeByte;
            entry.type = static_cast<FileChangeType>(typeByte);
            cmd.changes.append(entry);
        }

        fmInfo() << "WorkerPipe:changes:" << cmd.changes.size();
        emit processFileChangesReceived(cmd);
        break;
    }

    case CommandType::ProcessFileList: {
        ProcessFileListCommand cmd;
        quint8 operationByte = 0;
        messageStream >> cmd.fileList >> operationByte >> cmd.silent;
        cmd.operation = static_cast<FileListOperationType>(operationByte);

        fmInfo() << "WorkerPipe: ProcessFileList - files:" << cmd.fileList.size() << "operation:" << static_cast<int>(cmd.operation)
                 << "silent:" << cmd.silent;
        emit processFileListReceived(cmd);
        break;
    }

    case CommandType::ProcessFileMoves: {
        ProcessFileMovesCommand cmd;

        qint32 moveCount = 0;
        messageStream >> moveCount;

        for (qint32 i = 0; i < moveCount; ++i) {
            QString fromPath, toPath;
            messageStream >> fromPath >> toPath;
            cmd.movedFiles.insert(fromPath, toPath);
        }

        messageStream >> cmd.silent;

        fmInfo() << "WorkerPipe: ProcessFileMoves - moves:" << cmd.movedFiles.size() << "silent:" << cmd.silent;
        emit processFileMovesReceived(cmd);
        break;
    }

    default:
        fmWarning() << "WorkerPipe: Unknown command type:" << static_cast<int>(cmdType);
        break;
    }
}

bool WorkerPipe::sendStatus(TextIndexCreatorStatus status, const QString &filePath, const QByteArray &data)
{
    if (!d->initialized) {
        fmWarning() << "WorkerPipe::sendStatus: Not initialized";
        return false;
    }

    // Serialize the message
    QByteArray messageData;
    QDataStream messageStream(&messageData, QIODevice::WriteOnly);
    messageStream << static_cast<quint8>(status);

    QByteArray packetData;
    QDataStream packetStream(&packetData, QIODevice::WriteOnly);
    packetStream << static_cast<qint32>(messageData.size());
    packetData.append(messageData);

    if (!writePacket(packetData)) {
        fmCritical() << "WorkerPipe::sendStatus: Failed to write packet";
        return false;
    }

    fmDebug() << "WorkerPipe::sendStatus: Sent status" << statusToString(status)
              << "for" << filePath << "data size:" << data.size();

    return true;
}

bool WorkerPipe::sendProgress(qint64 processedCount, qint64 totalCount, const QString &currentFile)
{
    if (!d->initialized) {
        fmWarning() << "WorkerPipe::sendProgress: Not initialized";
        return false;
    }

    QByteArray messageData;
    QDataStream messageStream(&messageData, QIODevice::WriteOnly);
    messageStream << static_cast<quint8>(TextIndexCreatorStatus::Progress);
    messageStream << processedCount << totalCount << currentFile;

    QByteArray packetData;
    QDataStream packetStream(&packetData, QIODevice::WriteOnly);
    packetStream << static_cast<qint32>(messageData.size());
    packetData.append(messageData);

    if (!writePacket(packetData)) {
        fmCritical() << "WorkerPipe::sendProgress: Failed to write packet";
        return false;
    }

    fmDebug() << "WorkerPipe::sendProgress: Sent progress" << processedCount << "/" << totalCount;
    return true;
}

bool WorkerPipe::sendTaskFinished(bool success, qint64 processedCount, const QString &errorMessage,
                                   bool interrupted, bool useAnything, bool fatal)
{
    if (!d->initialized) {
        fmWarning() << "WorkerPipe::sendTaskFinished: Not initialized";
        return false;
    }

    QByteArray messageData;
    QDataStream messageStream(&messageData, QIODevice::WriteOnly);
    messageStream << static_cast<quint8>(TextIndexCreatorStatus::TaskFinished);
    messageStream << success << interrupted << useAnything << fatal;
    messageStream << processedCount << errorMessage;

    QByteArray packetData;
    QDataStream packetStream(&packetData, QIODevice::WriteOnly);
    packetStream << static_cast<qint32>(messageData.size());
    packetData.append(messageData);

    if (!writePacket(packetData)) {
        fmCritical() << "WorkerPipe::sendTaskFinished: Failed to write packet";
        return false;
    }

    fmInfo() << "WorkerPipe::sendTaskFinished: Sent task finished - success:" << success
             << "interrupted:" << interrupted << "useAnything:" << useAnything
             << "fatal:" << fatal << "processed:" << processedCount;
    return true;
}

bool WorkerPipe::sendFileProcessed(const QString &filePath, bool success)
{
    if (!d->initialized) {
        fmWarning() << "WorkerPipe::sendFileProcessed: Not initialized";
        return false;
    }

    QByteArray messageData;
    QDataStream messageStream(&messageData, QIODevice::WriteOnly);
    messageStream << static_cast<quint8>(TextIndexCreatorStatus::FileProcessed);
    messageStream << filePath << success;

    QByteArray packetData;
    QDataStream packetStream(&packetData, QIODevice::WriteOnly);
    packetStream << static_cast<qint32>(messageData.size());
    packetData.append(messageData);

    if (!writePacket(packetData)) {
        fmCritical() << "WorkerPipe::sendFileProcessed: Failed to write packet";
        return false;
    }

    fmDebug() << "WorkerPipe::sendFileProcessed: Sent file processed -" << filePath
              << "success:" << success;
    return true;
}

bool WorkerPipe::writePacket(const QByteArray &packetData)
{
    qint64 totalWritten = 0;
    while (totalWritten < packetData.size()) {
        const ssize_t bytesWritten = ::write(d->outputFd,
                                             packetData.constData() + totalWritten,
                                             static_cast<size_t>(packetData.size() - totalWritten));
        if (bytesWritten <= 0) {
            return false;
        }

        totalWritten += bytesWritten;
    }

    return true;
}

bool WorkerPipe::hasPendingPartialMessage() const
{
    return d->waitingForComplete || !d->inputBuffer.isEmpty();
}

bool WorkerPipe::setupOutputChannel()
{
    d->outputFd = ::dup(STDOUT_FILENO);
    if (d->outputFd < 0) {
        return false;
    }

    const int nullFd = ::open("/dev/null", O_WRONLY | O_CLOEXEC);
    if (nullFd < 0) {
        ::close(d->outputFd);
        d->outputFd = -1;
        return false;
    }

    // Keep a dedicated fd for binary IPC and sink accidental stdout writes so
    // they cannot be misread as protocol frames or flood the controller's
    // stderr log stream after redirection.
    if (::dup2(nullFd, STDOUT_FILENO) < 0) {
        ::close(nullFd);
        ::close(d->outputFd);
        d->outputFd = -1;
        return false;
    }

    ::close(nullFd);

    if (::fflush(stdout) != 0 && errno != EBADF) {
        fmWarning() << "WorkerPipe::setupOutputChannel: Failed to flush stdout after redirect";
    }

    return true;
}

TEXTINDEX_CREATOR_IPC_END_NAMESPACE
