// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CONTROLLERPIPE_H
#define CONTROLLERPIPE_H

#include "textindex_creator_ipc_global.h"
#include "textindexcreatortypes.h"

#include <QObject>
#include <QProcess>
#include <QSharedPointer>

TEXTINDEX_CREATOR_IPC_BEGIN_NAMESPACE

class ControllerPipePrivate;

/**
 * @brief ControllerPipe manages communication with the extractor subprocess.
 *
 * This class is used by the main process (e.g., textindex service) to:
 * 1. Start the extractor subprocess
 * 2. Send batch extraction requests
 * 3. Receive extraction results via signals
 * 4. Handle subprocess lifecycle (crashes, timeouts)
 */
class ControllerPipe : public QObject
{
    Q_OBJECT

public:
    explicit ControllerPipe(QObject *parent = nullptr);
    ~ControllerPipe() override;

    /**
     * @brief Start the extractor subprocess
     * @param extractorPath Path to the extractor executable
     * @param pluginPath Optional path to plugin directory
     * @return true if started successfully
     */
    bool start(const QString &textIndexCreatorPath, const QString &pluginPath = QString());

    /**
     * @brief Stop the extractor subprocess
     */
    void stop();

    /**
     * @brief Check if the extractor is running
     */
    bool isRunning() const;

    // === New methods for index operations ===

    /**
     * @brief Send create index command
     * @param sourcePath Source directory to index
     * @param indexPath Index storage path
     * @param extensions Supported file extensions (optional)
     * @param silent Whether to run in silent mode (no progress signals)
     * @return true if the request was sent successfully
     */
    bool createIndex(const QString &sourcePath, bool silent = false);

    /**
     * @brief Send update index command
     * @param sourcePath Source directory to update
     * @param indexPath Index storage path
     * @param extensions Supported file extensions (optional)
     * @param silent Whether to run in silent mode (no progress signals)
     * @return true if the request was sent successfully
     */
    bool updateIndex(const QString &sourcePath, bool silent = false);

    /**
     * @brief Send stop task command
     * @return true if the request was sent successfully
     */
    bool stopTask();

    /**
     * @brief Send process file changes command
     * @param changes List of file changes
     * @return true if the request was sent successfully
     */
    bool processFileChanges(const QVector<FileChangeEntry> &changes);

    /**
     * @brief Send process file list command
     * @param fileList List of file paths to process
     * @param operation Operation type (create/update/remove)
     * @param silent Whether to run in silent mode (no progress signals)
     * @return true if the request was sent successfully
     */
    bool processFileList(const QStringList &fileList,
                         FileListOperationType operation, bool silent = false);

    /**
     * @brief Send process file moves command
     * @param movedFiles Hash mapping from source paths to destination paths
     * @param silent Whether to run in silent mode (no progress signals)
     * @return true if the request was sent successfully
     */
    bool processFileMoves(const QHash<QString, QString> &movedFiles, bool silent = false);

    /**
     * @brief Get the process ID of the extractor
     * @return Process ID or -1 if not running
     */
    qint64 processId() const;

signals:
    /**
     * @brief Emitted when progress is reported during indexing
     * @param processedCount Number of files processed
     * @param totalCount Total number of files
     * @param currentFile Current file being processed
     */
    void progressChanged(qint64 processedCount, qint64 totalCount, const QString &currentFile);

    /**
     * @brief Emitted when a task finishes
     * @param success Whether the task succeeded
     * @param processedCount Total files processed
     * @param errorMessage Error message if failed
     * @param interrupted Whether the task was interrupted by user
     * @param useAnything Whether ANYTHING backend was used
     * @param fatal Whether a fatal error occurred
     */
    void taskFinished(bool success, qint64 processedCount, const QString &errorMessage,
                      bool interrupted, bool useAnything, bool fatal);

    /**
     * @brief Emitted when a single file is processed during indexing
     * @param filePath The processed file path
     * @param success Whether processing succeeded
     */
    void fileProcessed(const QString &filePath, bool success);

    /**
     * @brief Emitted when the extractor process crashes
     */
    void processCrashed();

    /**
     * @brief Emitted when the extractor process exits normally
     */
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);

    /**
     * @brief Emitted when an error occurs
     * @param error Error description
     */
    void errorOccurred(const QString &error);

private:
    void handleProcessOutput();
    void processInputBuffer();
    void handleStatusMessage(const QByteArray &messageData);
    bool hasPendingPartialMessage() const;
    bool sendCommand(CommandType cmdType, const QByteArray &payload);

    QScopedPointer<ControllerPipePrivate> d;
};

TEXTINDEX_CREATOR_IPC_END_NAMESPACE

#endif   // CONTROLLERPIPE_H
