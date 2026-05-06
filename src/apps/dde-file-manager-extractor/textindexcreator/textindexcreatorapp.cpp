// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "textindexcreatorapp.h"
#include "profile/indexprofile.h"

#include <dfm-base/utils/processprioritymanager.h>

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>

// Register log category for TEXTINDEX_CREATOR_NAMESPACE
TEXTINDEX_CREATOR_BEGIN_NAMESPACE
DFM_LOG_REISGER_CATEGORY(TEXTINDEX_CREATOR_NAMESPACE)
TEXTINDEX_CREATOR_END_NAMESPACE

TEXTINDEX_CREATOR_USE_NAMESPACE

EXTRACTOR_PLUGIN_BEGIN_NAMESPACE

TextIndexCreatorApp::TextIndexCreatorApp(QObject *parent)
    : QObject(parent), m_pluginLoader(new PluginLoader(this)), m_workerPipe(new TEXTINDEX_CREATOR_IPC_NAMESPACE::WorkerPipe(this)),
      m_idleTimer(new QTimer(this)),
      runtime(new IndexRuntime(IndexProfile::content(), this))
{
    m_idleTimer->setSingleShot(true);
    connect(m_idleTimer, &QTimer::timeout, this, []() {
        fmInfo() << "TextIndexCreatorApp: No batch received for" << kIdleTimeoutMs
                 << "ms, exiting idle extractor process";
        QCoreApplication::quit();
    });
}

TextIndexCreatorApp::~TextIndexCreatorApp()
{
}

bool TextIndexCreatorApp::initialize(const QString &pluginPath)
{
    return runtime->initialize(pluginPath);
    fmInfo() << "TextIndexCreatorApp: Initializing with plugin path:" << pluginPath;

    // Lower process priority to avoid impacting user experience
    DFMBASE_NAMESPACE::ProcessPriorityManager::lowerAllAvailablePriorities(true);

    // Load plugins
    int pluginCount = m_pluginLoader->loadPlugins(pluginPath);
    if (pluginCount == 0) {
        fmWarning() << "TextIndexCreatorApp: No plugins loaded";
        return false;
    }

    fmInfo() << "TextIndexCreatorApp: Initialization complete, loaded" << pluginCount << "plugins";
    return true;
}

void TextIndexCreatorApp::run()
{
    fmInfo() << "TextIndexCreatorApp: Starting main loop";

    if (!m_workerPipe->initialize()) {
        fmCritical() << "TextIndexCreatorApp: Failed to initialize worker pipe";
        return;
    }

    // Connect signals for content extraction
    connect(m_workerPipe.data(), &TEXTINDEX_CREATOR_IPC_NAMESPACE::WorkerPipe::activityDetected,
            this, &TextIndexCreatorApp::resetIdleTimer);

    connect(m_workerPipe.data(), &TEXTINDEX_CREATOR_IPC_NAMESPACE::WorkerPipe::stdinClosed,
            this, []() {
                fmInfo() << "TextIndexCreatorApp: Stdin closed, exiting";
                QCoreApplication::quit();
            });

    // === Connect IPC signals to TaskManager for index operations ===

    // Handle CreateIndex command
    connect(m_workerPipe.data(), &TEXTINDEX_CREATOR_IPC_NAMESPACE::WorkerPipe::createIndexReceived,
            this, [this](const TEXTINDEX_CREATOR_IPC_NAMESPACE::CreateIndexCommand &cmd) {
        fmInfo() << "TextIndexCreatorApp: Received CreateIndex command - source:" << cmd.sourcePath
                 << "silent:" << cmd.silent;
        if (runtime && runtime->taskManager()) {
            runtime->taskManager()->startTask(TEXTINDEX_CREATOR_NAMESPACE::IndexTask::Type::Create, {cmd.sourcePath}, cmd.silent);
        }
    });

    // Handle UpdateIndex command
    connect(m_workerPipe.data(), &TEXTINDEX_CREATOR_IPC_NAMESPACE::WorkerPipe::updateIndexReceived,
            this, [this](const TEXTINDEX_CREATOR_IPC_NAMESPACE::UpdateIndexCommand &cmd) {
        fmInfo() << "TextIndexCreatorApp: Received UpdateIndex command - source:" << cmd.sourcePath
                 << "silent:" << cmd.silent;
        if (runtime && runtime->taskManager()) {
            runtime->taskManager()->startTask(TEXTINDEX_CREATOR_NAMESPACE::IndexTask::Type::Update, {cmd.sourcePath}, cmd.silent);
        }
    });

    // Handle StopTask command
    connect(m_workerPipe.data(), &TEXTINDEX_CREATOR_IPC_NAMESPACE::WorkerPipe::stopTaskReceived,
            this, [this]() {
        fmInfo() << "TextIndexCreatorApp: Received StopTask command";
        if (runtime && runtime->taskManager()) {
            runtime->taskManager()->stopCurrentTask();
        }
    });

    // Handle ProcessFileChanges command
    connect(m_workerPipe.data(), &TEXTINDEX_CREATOR_IPC_NAMESPACE::WorkerPipe::processFileChangesReceived,
            this, [this](const TEXTINDEX_CREATOR_IPC_NAMESPACE::ProcessFileChangesCommand &cmd) {
        fmInfo() << "TextIndexCreatorApp: Received ProcessFileChanges command - changes:" << cmd.changes.size();
        if (runtime && runtime->taskManager()) {
            // Convert FileChangeEntry to appropriate task
            QStringList createdFiles, modifiedFiles, deletedFiles;
            for (const auto &change : cmd.changes) {
                switch (change.type) {
                case TEXTINDEX_CREATOR_IPC_NAMESPACE::FileChangeType::Created:
                    createdFiles.append(change.filePath);
                    break;
                case TEXTINDEX_CREATOR_IPC_NAMESPACE::FileChangeType::Modified:
                    modifiedFiles.append(change.filePath);
                    break;
                case TEXTINDEX_CREATOR_IPC_NAMESPACE::FileChangeType::Deleted:
                    deletedFiles.append(change.filePath);
                    break;
                }
            }

            // Process in order: delete first, then create, then modify
            if (!deletedFiles.isEmpty()) {
                runtime->taskManager()->startFileListTask(
                    TEXTINDEX_CREATOR_NAMESPACE::IndexTask::Type::RemoveFileList, deletedFiles, true);
            }
            if (!createdFiles.isEmpty()) {
                runtime->taskManager()->startFileListTask(
                    TEXTINDEX_CREATOR_NAMESPACE::IndexTask::Type::CreateFileList, createdFiles, true);
            }
            if (!modifiedFiles.isEmpty()) {
                runtime->taskManager()->startFileListTask(
                    TEXTINDEX_CREATOR_NAMESPACE::IndexTask::Type::UpdateFileList, modifiedFiles, true);
            }
        }
    });

    // Handle ProcessFileList command
    connect(m_workerPipe.data(), &TEXTINDEX_CREATOR_IPC_NAMESPACE::WorkerPipe::processFileListReceived,
            this, [this](const TEXTINDEX_CREATOR_IPC_NAMESPACE::ProcessFileListCommand &cmd) {
        fmInfo() << "TextIndexCreatorApp: Received ProcessFileList command - "
                 << "files:" << cmd.fileList.size() << "operation:" << static_cast<int>(cmd.operation)
                 << "silent:" << cmd.silent;
        if (runtime && runtime->taskManager()) {
            TEXTINDEX_CREATOR_NAMESPACE::IndexTask::Type taskType;
            switch (cmd.operation) {
            case TEXTINDEX_CREATOR_IPC_NAMESPACE::FileListOperationType::Create:
                taskType = TEXTINDEX_CREATOR_NAMESPACE::IndexTask::Type::CreateFileList;
                break;
            case TEXTINDEX_CREATOR_IPC_NAMESPACE::FileListOperationType::Update:
                taskType = TEXTINDEX_CREATOR_NAMESPACE::IndexTask::Type::UpdateFileList;
                break;
            case TEXTINDEX_CREATOR_IPC_NAMESPACE::FileListOperationType::Remove:
                taskType = TEXTINDEX_CREATOR_NAMESPACE::IndexTask::Type::RemoveFileList;
                break;
            default:
                fmWarning() << "TextIndexCreatorApp: Unknown file list operation type:" << static_cast<int>(cmd.operation);
                return;
            }
            runtime->taskManager()->startFileListTask(taskType, cmd.fileList, cmd.silent);
        }
    });

    // Handle ProcessFileMoves command
    connect(m_workerPipe.data(), &TEXTINDEX_CREATOR_IPC_NAMESPACE::WorkerPipe::processFileMovesReceived,
            this, [this](const TEXTINDEX_CREATOR_IPC_NAMESPACE::ProcessFileMovesCommand &cmd) {
        fmInfo() << "TextIndexCreatorApp: Received ProcessFileMoves command - moves:" << cmd.movedFiles.size() << "silent:" << cmd.silent;
        if (runtime && runtime->taskManager()) {
            runtime->taskManager()->startFileMoveTask(cmd.movedFiles, cmd.silent);
        }
    });

    // === Connect TaskManager signals to WorkerPipe for status reporting ===

    if (runtime && runtime->taskManager()) {
        // Report progress
        connect(runtime->taskManager(), &TEXTINDEX_CREATOR_NAMESPACE::TaskManager::taskProgressChanged,
                this, [this](const QString &type, const QString &path, qint64 count, qint64 total) {
            Q_UNUSED(type);
            m_workerPipe->sendProgress(count, total, path);
        });

        // Report task completion
        connect(runtime->taskManager(), &TEXTINDEX_CREATOR_NAMESPACE::TaskManager::taskFinished,
                this, [this](const QString &type, const QString &path, bool success,
                             bool interrupted, bool useAnything, bool fatal) {
            fmInfo() << "TextIndexCreatorApp: Task finished - type:" << type << "path:" << path
                     << "success:" << success << "interrupted:" << interrupted
                     << "useAnything:" << useAnything << "fatal:" << fatal;
            m_workerPipe->sendTaskFinished(success, 0, success ? QString() : QStringLiteral("Task failed"),
                                           interrupted, useAnything, fatal);
        });
    }

    resetIdleTimer();
    fmInfo() << "TextIndexCreatorApp: Ready to process requests";

    // Enter event loop
    QCoreApplication::exec();

    fmInfo() << "TextIndexCreatorApp: Exiting";
}

void TextIndexCreatorApp::resetIdleTimer()
{
    m_idleTimer->start(kIdleTimeoutMs);
}

EXTRACTOR_PLUGIN_END_NAMESPACE
