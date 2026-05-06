// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TEXTINDEXCREATORSERVICE_H
#define TEXTINDEXCREATORSERVICE_H

#include "service_textindex_global.h"

// Include textindexcreator types from libextractor
#include <textindexcreatortypes.h>

#include <QObject>
#include <QString>
#include <QVector>
#include <QHash>
#include <QUuid>
#include <QProcess>

// Forward declaration for extractor namespace types
namespace EXTRACTOR_NAMESPACE {
class ControllerPipe;
}

SERVICETEXTINDEX_BEGIN_NAMESPACE

/**
 * @brief Result of an index operation
 */
struct IndexOperationResult
{
    bool success = false;
    bool interrupted = false;       // Whether the task was interrupted by user
    bool useAnything = false;       // Whether ANYTHING backend was used
    bool fatal = false;             // Whether a fatal error occurred
    qint64 processedCount = 0;
    QString errorMessage;
    QString type;
    QString filePath;
};

/**
 * @brief Progress information for long-running operations
 */
struct OperationProgress
{
    qint64 processedCount = 0;
    qint64 totalCount = 0;
    QString currentFile;
    QString type;
    QString filePath;
};

/**
 * @brief TextIndexCreatorService provides asynchronous communication with the extractor subprocess.
 *
 * This service handles all communication with dde-file-manager-textindex-creator process:
 * - Index creation
 * - Index updates
 * - File list processing
 * - File move processing
 *
 * All operations are asynchronous and results are delivered via signals.
 */
class TextIndexCreatorService : public QObject
{
    Q_OBJECT

public:
    explicit TextIndexCreatorService(QObject *parent = nullptr);
    ~TextIndexCreatorService() override;

    Q_DISABLE_COPY(TextIndexCreatorService)

    /**
     * @brief Initialize the service (starts the extractor process)
     * @return true if initialization succeeded
     */
    bool initialize();

    /**
     * @brief Shutdown the service and stop the extractor process
     */
    void shutdown();

    /**
     * @brief Check if the extractor process is running
     */
    bool isRunning() const;

    // === Asynchronous Index Operations ===

    /**
     * @brief Create a full-text index for a directory
     * @param sourcePath Source directory to index
     * @param silent Whether to run in silent mode (no progress signals)
     * @return Request ID for tracking the operation
     */
    QString createIndex(const QString &sourcePath, bool silent = false);

    /**
     * @brief Update an existing index
     * @param sourcePath Source directory to update
     * @param silent Whether to run in silent mode (no progress signals)
     * @return Request ID for tracking the operation
     */
    QString updateIndex(const QString &sourcePath, bool silent = false);

    /**
     * @brief Process a list of files (create/update/remove)
     * @param fileList List of file paths to process
     * @param operation Operation type (create/update/remove)
     * @param silent Whether to run in silent mode (no progress signals)
     * @return Request ID for tracking the operation
     */
    QString processFileList(const QStringList &fileList,
                            TEXTINDEX_CREATOR_IPC_NAMESPACE::FileListOperationType operation, bool silent = false);

    /**
     * @brief Process file moves (rename operations)
     * @param movedFiles Hash mapping from source paths to destination paths
     * @param silent Whether to run in silent mode (no progress signals)
     * @return Request ID for tracking the operation
     */
    QString processFileMoves(const QHash<QString, QString> &movedFiles, bool silent = false);

    /**
     * @brief Stop the current running task
     */
    void stopCurrentTask();

signals:
    /**
     * @brief Emitted when an index operation completes
     * @param requestId The request ID
     * @param result The operation result
     */
    void indexOperationFinished(const QString &requestId, const IndexOperationResult &result);

    /**
     * @brief Emitted periodically during long-running operations
     * @param requestId The request ID
     * @param progress Progress information
     */
    void progressReported(const QString &requestId, const OperationProgress &progress);

    /**
     * @brief Emitted when the extractor process crashes
     */
    void processCrashed();

    /**
     * @brief Emitted when an error occurs
     * @param error Error description
     */
    void errorOccurred(const QString &error);

private:
    void setupConnections();
    QString generateRequestId();
    void handleTaskFinished(bool success, qint64 processedCount, const QString &errorMessage,
                            bool interrupted, bool useAnything, bool fatal);
    void handleProgressChanged(qint64 processedCount, qint64 totalCount, const QString &currentFile);
    void handleProcessCrashed();
    void handleErrorOccurred(const QString &error);
    void handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

    class TextIndexCreatorServicePrivate;
    QScopedPointer<TextIndexCreatorServicePrivate> d;
};

SERVICETEXTINDEX_END_NAMESPACE

#endif   // TEXTINDEXCREATORSERVICE_H
