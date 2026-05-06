// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef EXTRACTORTYPES_H
#define EXTRACTORTYPES_H

#include "textindex_creator_ipc_global.h"

#include <QByteArray>
#include <QString>
#include <QVector>
#include <QHash>

TEXTINDEX_CREATOR_IPC_BEGIN_NAMESPACE

/**
 * @brief Command types for controller -> worker communication
 */
enum class CommandType : quint8 {
    ExtractBatch = 0,       // Extract content from a batch of files
    CreateIndex = 1,        // Create index for a directory
    UpdateIndex = 2,        // Update index for a directory
    StopTask = 3,           // Stop current task
    ProcessFileChanges = 4, // Process file changes (create/update/delete)
    ProcessFileList = 5,    // Process a list of files (create/update/remove)
    ProcessFileMoves = 6    // Process file moves (rename operations)
};

/**
 * @brief Status codes for extractor IPC communication
 */
enum class TextIndexCreatorStatus : quint8 {
    // New status codes for index operations
    Progress = 'P',         // Progress report (processed count, total count)
    TaskFinished = 'T',     // Task finished (success, error message)
    FileProcessed = 'p'     // Single file processed during indexing
};

/**
 * @brief Convert ExtractorStatus to string for logging
 */
inline const char* statusToString(TextIndexCreatorStatus status)
{
    switch (status) {
    case TextIndexCreatorStatus::Progress:
        return "Progress";
    case TextIndexCreatorStatus::TaskFinished:
        return "TaskFinished";
    case TextIndexCreatorStatus::FileProcessed:
        return "FileProcessed";
    default:
        return "Unknown";
    }
}

/**
 * @brief Convert CommandType to string for logging
 */
inline const char* commandToString(CommandType cmd)
{
    switch (cmd) {
    case CommandType::ExtractBatch:
        return "ExtractBatch";
    case CommandType::CreateIndex:
        return "CreateIndex";
    case CommandType::UpdateIndex:
        return "UpdateIndex";
    case CommandType::StopTask:
        return "StopTask";
    case CommandType::ProcessFileChanges:
        return "ProcessFileChanges";
    case CommandType::ProcessFileList:
        return "ProcessFileList";
    case CommandType::ProcessFileMoves:
        return "ProcessFileMoves";
    default:
        return "Unknown";
    }
}

/**
 * @brief Result of a single file extraction
 */
struct ExtractionResult {
    QString filePath;
    bool success = false;
    QByteArray data;
    QString error;
};

/**
 * @brief Batch extraction results
 */
struct BatchResult {
    QVector<ExtractionResult> results;
    int successCount = 0;
    int failureCount = 0;
};

/**
 * @brief CreateIndex command data
 */
struct CreateIndexCommand {
    QString sourcePath;     // Source directory to index
    bool silent = false;    // Whether to run in silent mode (no progress signals)
};

/**
 * @brief UpdateIndex command data
 */
struct UpdateIndexCommand {
    QString sourcePath;     // Source directory to update
    bool silent = false;    // Whether to run in silent mode (no progress signals)
};

/**
 * @brief File change type for ProcessFileChanges
 */
enum class FileChangeType : quint8 {
    Created = 0,
    Modified = 1,
    Deleted = 2
};

/**
 * @brief Single file change entry
 */
struct FileChangeEntry {
    QString filePath;
    FileChangeType type;
};

/**
 * @brief ProcessFileChanges command data
 */
struct ProcessFileChangesCommand {
    QVector<FileChangeEntry> changes;   // List of file changes
};

/**
 * @brief File list operation type
 */
enum class FileListOperationType : quint8 {
    Create = 0,     // Create documents for new files
    Update = 1,     // Update existing documents
    Remove = 2      // Remove documents from index
};

/**
 * @brief ProcessFileList command data
 */
struct ProcessFileListCommand {
    QStringList fileList;               // List of file paths
    FileListOperationType operation;    // Operation type (create/update/remove)
    bool silent = false;                // Whether to run in silent mode (no progress signals)
};

/**
 * @brief ProcessFileMoves command data
 */
struct ProcessFileMovesCommand {
    QHash<QString, QString> movedFiles; // fromPath -> toPath mapping
    bool silent = false;                // Whether to run in silent mode (no progress signals)
};

/**
 * @brief Progress report data
 */
struct ProgressReport {
    qint64 processedCount = 0;  // Number of files processed
    qint64 totalCount = 0;      // Total number of files to process
    QString currentFile;        // Current file being processed (optional)
};

/**
 * @brief Task finished report data
 */
struct TaskFinishedReport {
    bool success = false;           // Whether the task succeeded
    bool interrupted = false;       // Whether the task was interrupted by user
    bool useAnything = false;       // Whether ANYTHING backend was used
    bool fatal = false;             // Whether a fatal error occurred
    qint64 processedCount = 0;      // Total files processed
    QString errorMessage;           // Error message if failed
};

TEXTINDEX_CREATOR_IPC_END_NAMESPACE

// Q_DECLARE_METATYPE for structs used in signals/slots
Q_DECLARE_METATYPE(TEXTINDEX_CREATOR_IPC_NAMESPACE::CreateIndexCommand)
Q_DECLARE_METATYPE(TEXTINDEX_CREATOR_IPC_NAMESPACE::UpdateIndexCommand)
Q_DECLARE_METATYPE(TEXTINDEX_CREATOR_IPC_NAMESPACE::ProcessFileChangesCommand)
Q_DECLARE_METATYPE(TEXTINDEX_CREATOR_IPC_NAMESPACE::FileChangeEntry)
Q_DECLARE_METATYPE(TEXTINDEX_CREATOR_IPC_NAMESPACE::ProgressReport)
Q_DECLARE_METATYPE(TEXTINDEX_CREATOR_IPC_NAMESPACE::TaskFinishedReport)
Q_DECLARE_METATYPE(TEXTINDEX_CREATOR_IPC_NAMESPACE::ProcessFileListCommand)
Q_DECLARE_METATYPE(TEXTINDEX_CREATOR_IPC_NAMESPACE::ProcessFileMovesCommand)

#endif   // EXTRACTORTYPES_H
