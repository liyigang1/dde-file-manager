// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WORKERPIPE_H
#define WORKERPIPE_H

#include "textindex_creator_ipc_global.h"
#include "textindexcreatortypes.h"

#include <QObject>

TEXTINDEX_CREATOR_IPC_BEGIN_NAMESPACE

class WorkerPipePrivate;

/**
 * @brief WorkerPipe handles communication from the extractor subprocess side.
 *
 * This class is used by the extractor subprocess to:
 * 1. Receive extraction requests from stdin
 * 2. Send status updates and results to stdout
 * 3. Handle message framing with QDataStream transactions
 */
class WorkerPipe : public QObject
{
    Q_OBJECT

public:
    explicit WorkerPipe(QObject *parent = nullptr);
    ~WorkerPipe() override;

    /**
     * @brief Initialize the worker pipe
     * @return true if initialization succeeded
     */
    bool initialize();

    /**
     * @brief Send a status message to the controller
     * @param status The status code
     * @param filePath Optional file path (for Started/Finished/Failed/Data)
     * @param data Optional data (for Data status)
     * @return true if sent successfully
     */
    bool sendStatus(TextIndexCreatorStatus status, const QString &filePath = QString(),
                    const QByteArray &data = QByteArray());

    /**
     * @brief Send extraction started notification
     */
    bool sendStarted(const QString &filePath);

    /**
     * @brief Send extraction finished with data
     */
    bool sendData(const QString &filePath, const QByteArray &data);

    /**
     * @brief Send extraction failed notification
     */
    bool sendFailed(const QString &filePath, const QString &error = QString());

    /**
     * @brief Send batch done notification
     */
    bool sendBatchDone();

    // === New send methods for index operations ===

    /**
     * @brief Send progress report during indexing
     * @param processedCount Number of files processed
     * @param totalCount Total number of files
     * @param currentFile Current file being processed (optional)
     */
    bool sendProgress(qint64 processedCount, qint64 totalCount, const QString &currentFile = QString());

    /**
     * @brief Send task finished notification
     * @param success Whether the task succeeded
     * @param processedCount Total files processed
     * @param errorMessage Error message if failed
     * @param interrupted Whether the task was interrupted by user
     * @param useAnything Whether ANYTHING backend was used
     * @param fatal Whether a fatal error occurred
     */
    bool sendTaskFinished(bool success, qint64 processedCount, const QString &errorMessage = QString(),
                          bool interrupted = false, bool useAnything = false, bool fatal = false);

    /**
     * @brief Send single file processed notification during indexing
     * @param filePath The processed file path
     * @param success Whether processing succeeded
     */
    bool sendFileProcessed(const QString &filePath, bool success);

signals:
    /**
     * @brief Emitted when a complete batch request is received from stdin.
     */
    void activityDetected();

    /**
     * @brief Emitted when a batch request is received
     * @param filePaths List of files to extract
     */
    void batchReceived(const QVector<QString> &filePaths);

    // === New signals for index commands ===

    /**
     * @brief Emitted when create index command is received
     * @param command CreateIndex command data
     */
    void createIndexReceived(const CreateIndexCommand &command);

    /**
     * @brief Emitted when update index command is received
     * @param command UpdateIndex command data
     */
    void updateIndexReceived(const UpdateIndexCommand &command);

    /**
     * @brief Emitted when stop task command is received
     */
    void stopTaskReceived();

    /**
     * @brief Emitted when process file changes command is received
     * @param command ProcessFileChanges command data
     */
    void processFileChangesReceived(const ProcessFileChangesCommand &command);

    /**
     * @brief Emitted when process file list command is received
     * @param command ProcessFileList command data
     */
    void processFileListReceived(const ProcessFileListCommand &command);

    /**
     * @brief Emitted when process file moves command is received
     * @param command ProcessFileMoves command data
     */
    void processFileMovesReceived(const ProcessFileMovesCommand &command);

    /**
     * @brief Emitted when stdin is closed (parent process terminated)
     */
    void stdinClosed();

private:
    bool readFromStdin();
    void processInputBuffer();
    void handleCommand(CommandType cmdType, const QByteArray &messageData);
    bool writePacket(const QByteArray &packetData);
    bool hasPendingPartialMessage() const;
    bool setupOutputChannel();

    QScopedPointer<WorkerPipePrivate> d;
};

TEXTINDEX_CREATOR_IPC_END_NAMESPACE

#endif   // WORKERPIPE_H
