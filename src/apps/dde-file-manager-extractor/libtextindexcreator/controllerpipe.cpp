// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "controllerpipe.h"

#include <QDataStream>
#include <QProcess>
#include <QTimer>

TEXTINDEX_CREATOR_IPC_BEGIN_NAMESPACE

class ControllerPipePrivate
{
public:
    QProcess *process = nullptr;
    QByteArray inputBuffer;
    bool waitingForComplete = false;
    qint32 expectedSize = 0;

    void clearState()
    {
        inputBuffer.clear();
        waitingForComplete = false;
        expectedSize = 0;
    }
};

ControllerPipe::ControllerPipe(QObject *parent)
    : QObject(parent), d(new ControllerPipePrivate())
{
}

ControllerPipe::~ControllerPipe()
{
    stop();
}

bool ControllerPipe::start(const QString &textIndexCreatorPath, const QString &pluginPath)
{
    if (isRunning()) {
        fmWarning() << "ControllerPipe::start: Process already running";
        return false;
    }

    if (textIndexCreatorPath.isEmpty()) {
        fmCritical() << "ControllerPipe::start: TextIndexCreator path is empty";
        emit errorOccurred("TextIndexCreator path is empty");
        return false;
    }

    d->process = new QProcess(this);

    // Connect process signals
    connect(d->process, &QProcess::readyReadStandardOutput,
            this, &ControllerPipe::handleProcessOutput);

    connect(d->process, &QProcess::errorOccurred,
            this, [this](QProcess::ProcessError error) {
                if (hasPendingPartialMessage()) {
                    fmWarning() << "ControllerPipe: Process error occurred with incomplete message."
                                << "buffer size:" << d->inputBuffer.size()
                                << "expected payload size:" << d->expectedSize;
                }
                fmCritical() << "ControllerPipe: Process error:" << error;
                emit errorOccurred(QString("Process error: %1").arg(static_cast<int>(error)));
            });

    connect(d->process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
                QProcess *finishedProcess = qobject_cast<QProcess *>(sender());
                if (hasPendingPartialMessage()) {
                    fmWarning() << "ControllerPipe: Process finished with incomplete message."
                                << "buffer size:" << d->inputBuffer.size()
                                << "expected payload size:" << d->expectedSize;
                }
                fmInfo() << "ControllerPipe: Process finished with exit code:" << exitCode
                         << "status:" << exitStatus;

                if (exitStatus == QProcess::CrashExit) {
                    emit processCrashed();
                }

                emit processFinished(exitCode, exitStatus);

                if (d->process == finishedProcess) {
                    d->process = nullptr;
                }

                if (finishedProcess) {
                    finishedProcess->deleteLater();
                }
            });

    // Prepare arguments
    QStringList arguments;
    if (!pluginPath.isEmpty()) {
        arguments << "--plugin-path" << pluginPath;
    }

    // Start the process
    fmInfo() << "ControllerPipe: Starting TextIndexCreator:" << textIndexCreatorPath
             << "arguments:" << arguments;

    d->process->start(textIndexCreatorPath, arguments, QIODevice::ReadWrite);

    if (!d->process->waitForStarted(5000)) {
        fmCritical() << "ControllerPipe: Failed to start process:"
                     << d->process->errorString();
        emit errorOccurred(QString("Failed to start process: %1").arg(d->process->errorString()));
        d->process->deleteLater();
        d->process = nullptr;
        return false;
    }

    fmInfo() << "ControllerPipe: TextIndexCreator started successfully, PID:" << d->process->processId();
    return true;
}

void ControllerPipe::handleProcessOutput()
{
    QProcess *process = qobject_cast<QProcess *>(sender());
    if (!process) {
        process = d->process;
    }
    if (!process) {
        return;
    }

    d->inputBuffer.append(process->readAllStandardOutput());
    processInputBuffer();
}

void ControllerPipe::processInputBuffer()
{
    while (true) {
        if (!d->waitingForComplete) {
            if (d->inputBuffer.size() < static_cast<int>(sizeof(qint32))) {
                return;
            }

            QDataStream sizeStream(d->inputBuffer);
            sizeStream >> d->expectedSize;
            if (d->expectedSize < 0) {
                fmCritical() << "ControllerPipe: Invalid message size:" << d->expectedSize;
                d->clearState();
                emit errorOccurred(QStringLiteral("Invalid TextIndexCreator message size"));
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

        handleStatusMessage(messageData);
    }
}

void ControllerPipe::handleStatusMessage(const QByteArray &messageData)
{
    QDataStream messageStream(messageData);
    quint8 statusByte = 0;
    messageStream >> statusByte;
    const TextIndexCreatorStatus status = static_cast<TextIndexCreatorStatus>(statusByte);

    QString filePath;
    QByteArray data;
    QString error;

    switch (status) {
    case TextIndexCreatorStatus::Progress: {
        qint64 processedCount = 0;
        qint64 totalCount = 0;
        QString currentFile;
        messageStream >> processedCount >> totalCount >> currentFile;
        fmDebug() << "ControllerPipe: Progress" << processedCount << "/" << totalCount;
        emit progressChanged(processedCount, totalCount, currentFile);
        break;
    }

    case TextIndexCreatorStatus::TaskFinished: {
        bool success = false;
        bool interrupted = false;
        bool useAnything = false;
        bool fatal = false;
        qint64 processedCount = 0;
        QString errorMessage;
        messageStream >> success >> interrupted >> useAnything >> fatal >> processedCount >> errorMessage;
        fmInfo() << "ControllerPipe: Task finished - success:" << success
                 << "interrupted:" << interrupted << "useAnything:" << useAnything
                 << "fatal:" << fatal << "processed:" << processedCount;
        emit taskFinished(success, processedCount, errorMessage, interrupted, useAnything, fatal);
        break;
    }

    case TextIndexCreatorStatus::FileProcessed: {
        bool success = false;
        messageStream >> filePath >> success;
        fmDebug() << "ControllerPipe: File processed -" << filePath << "success:" << success;
        emit fileProcessed(filePath, success);
        break;
    }

    default:
        fmWarning() << "ControllerPipe: Unknown status:" << static_cast<int>(status);
        break;
    }
}

void ControllerPipe::stop()
{
    if (d->process) {
        QProcess *process = d->process;
        d->process = nullptr;

        fmInfo() << "ControllerPipe: Stopping TextIndexCreator process";

        // Close write channel to signal EOF to the subprocess
        process->closeWriteChannel();

        // Wait for the process to finish gracefully
        if (!process->waitForFinished(3000)) {
            fmWarning() << "ControllerPipe: Process did not finish gracefully, terminating";
            process->terminate();

            if (!process->waitForFinished(2000)) {
                fmWarning() << "ControllerPipe: Process did not terminate, killing";
                process->kill();
                process->waitForFinished(1000);
            }
        }

        process->deleteLater();
    }

    d->clearState();
}

bool ControllerPipe::hasPendingPartialMessage() const
{
    return d->waitingForComplete || !d->inputBuffer.isEmpty();
}

bool ControllerPipe::isRunning() const
{
    return d->process && d->process->state() == QProcess::Running;
}

bool ControllerPipe::createIndex(const QString &sourcePath, bool silent)
{
    if (!isRunning()) {
        fmWarning() << "ControllerPipe::createIndex: Process not running";
        return false;
    }

    if (sourcePath.isEmpty()) {
        fmWarning() << "ControllerPipe::createIndex: Invalid paths";
        return false;
    }

    fmInfo() << "ControllerPipe::createIndex: Sending create index command - source:" << sourcePath
             << "silent:" << silent;

    QByteArray messageData;
    QDataStream messageStream(&messageData, QIODevice::WriteOnly);
    messageStream << static_cast<quint8>(CommandType::CreateIndex);
    messageStream << sourcePath << silent;

    return sendCommand(CommandType::CreateIndex, messageData);
}

bool ControllerPipe::updateIndex(const QString &sourcePath, bool silent)
{
    if (!isRunning()) {
        fmWarning() << "ControllerPipe::updateIndex: Process not running";
        return false;
    }

    if (sourcePath.isEmpty()) {
        fmWarning() << "ControllerPipe::updateIndex: Invalid paths";
        return false;
    }

    fmInfo() << "ControllerPipe::updateIndex: Sending update index command - source:" << sourcePath
             << "silent:" << silent;

    QByteArray messageData;
    QDataStream messageStream(&messageData, QIODevice::WriteOnly);
    messageStream << static_cast<quint8>(CommandType::UpdateIndex);
    messageStream << sourcePath << silent;

    return sendCommand(CommandType::UpdateIndex, messageData);
}

bool ControllerPipe::stopTask()
{
    if (!isRunning()) {
        fmWarning() << "ControllerPipe::stopTask: Process not running";
        return false;
    }

    fmInfo() << "ControllerPipe::stopTask: Sending stop task command";

    QByteArray messageData;
    QDataStream messageStream(&messageData, QIODevice::WriteOnly);
    messageStream << static_cast<quint8>(CommandType::StopTask);

    return sendCommand(CommandType::StopTask, messageData);
}

bool ControllerPipe::processFileChanges(const QVector<FileChangeEntry> &changes)
{
    if (!isRunning()) {
        fmWarning() << "ControllerPipe::processFileChanges: Process not running";
        return false;
    }

    if (changes.isEmpty()) {
        fmWarning() << "ControllerPipe::processFileChanges: Invalid parameters";
        return false;
    }

    fmInfo() << "ControllerPipe::processFileChanges: Sending" << changes.size();

    QByteArray messageData;
    QDataStream messageStream(&messageData, QIODevice::WriteOnly);
    messageStream << static_cast<quint8>(CommandType::ProcessFileChanges);
    messageStream << static_cast<qint32>(changes.size());

    for (const auto &change : changes) {
        messageStream << change.filePath << static_cast<quint8>(change.type);
    }

    return sendCommand(CommandType::ProcessFileChanges, messageData);
}

bool ControllerPipe::processFileList(const QStringList &fileList,
                                      FileListOperationType operation, bool silent)
{
    if (!isRunning()) {
        fmWarning() << "ControllerPipe::processFileList: Process not running";
        return false;
    }

    if (fileList.isEmpty()) {
        fmWarning() << "ControllerPipe::processFileList: Invalid parameters";
        return false;
    }

    fmInfo() << "ControllerPipe::processFileList: Sending" << fileList.size()
             << "operation:" << static_cast<int>(operation)
             << "silent:" << silent;

    QByteArray messageData;
    QDataStream messageStream(&messageData, QIODevice::WriteOnly);
    messageStream << static_cast<quint8>(CommandType::ProcessFileList);
    messageStream << fileList << static_cast<quint8>(operation) << silent;

    return sendCommand(CommandType::ProcessFileList, messageData);
}

bool ControllerPipe::processFileMoves(const QHash<QString, QString> &movedFiles, bool silent)
{
    if (!isRunning()) {
        fmWarning() << "ControllerPipe::processFileMoves: Process not running";
        return false;
    }

    if (movedFiles.isEmpty()) {
        fmWarning() << "ControllerPipe::processFileMoves: Invalid parameters";
        return false;
    }

    fmInfo() << "ControllerPipe::processFileMoves: Sending" << movedFiles.size()
             << "silent:" << silent;

    QByteArray messageData;
    QDataStream messageStream(&messageData, QIODevice::WriteOnly);
    messageStream << static_cast<quint8>(CommandType::ProcessFileMoves);
    messageStream << static_cast<qint32>(movedFiles.size());

    for (auto it = movedFiles.constBegin(); it != movedFiles.constEnd(); ++it) {
        messageStream << it.key() << it.value();
    }

    messageStream << silent;

    return sendCommand(CommandType::ProcessFileMoves, messageData);
}

bool ControllerPipe::sendCommand(CommandType cmdType, const QByteArray &payload)
{
    if (!d->process) {
        return false;
    }

    // Write size header + message
    QByteArray packetData;
    QDataStream packetStream(&packetData, QIODevice::WriteOnly);
    packetStream << static_cast<qint32>(payload.size());
    packetData.append(payload);

    qint64 bytesWritten = d->process->write(packetData);
    if (bytesWritten != packetData.size()) {
        fmCritical() << "ControllerPipe::sendCommand: Failed to write complete message for command"
                     << commandToString(cmdType);
        return false;
    }

    d->process->waitForBytesWritten();
    return true;
}

qint64 ControllerPipe::processId() const
{
    return d->process ? d->process->processId() : -1;
}

TEXTINDEX_CREATOR_IPC_END_NAMESPACE
