// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "taskmanager.h"
#include "utils/indexutility.h"

#include <QMetaType>
#include <QFile>
#include <QDir>
#include <QDateTime>

TEXTINDEX_CREATOR_USE_NAMESPACE

namespace {
void registerMetaTypes()
{
    static bool registered = false;
    if (!registered) {
        qRegisterMetaType<IndexTask::Type>();
        qRegisterMetaType<IndexTask::Type>("IndexTask::Type");
        qRegisterMetaType<TEXTINDEX_CREATOR_NAMESPACE::IndexTask::Type>();
        qRegisterMetaType<TEXTINDEX_CREATOR_NAMESPACE::IndexTask::Type>("TEXTINDEX_CREATOR_NAMESPACE::IndexTask::Type");
        qRegisterMetaType<HandlerResult>();
        registered = true;
        fmDebug() << "[TaskManager] Meta types registered successfully";
    }
}

}   // namespace

TaskManager::TaskManager(const IndexContext *context, QObject *parent)
    : QObject(parent),
      m_context(context)
{
    fmInfo() << "[TaskManager] Initializing TaskManager instance";
    registerMetaTypes();
    fmInfo() << "[TaskManager] TaskManager initialization completed";
}

TaskManager::~TaskManager()
{
    fmInfo() << "[TaskManager] Destroying TaskManager instance";
    if (currentTask) {
        fmInfo() << "[TaskManager] Stopping current task before destruction";
        stopCurrentTask();
    }

    if (workerThread.isRunning()) {
        fmInfo() << "[TaskManager] Stopping worker thread";
        workerThread.quit();
        if (!workerThread.wait(5000)) {
            fmWarning() << "[TaskManager] Worker thread did not stop within timeout, forcing termination";
            workerThread.terminate();
            workerThread.wait(1000);
        }
    }
    fmInfo() << "[TaskManager] TaskManager destroyed successfully";
}

bool TaskManager::startTask(IndexTask::Type type, const QString &path, bool silent)
{
    Q_ASSERT_X(type == IndexTask::Type::Create || type == IndexTask::Type::Update,
               "Type error", "Only create and update supported");

    fmInfo() << "[TaskManager::startTask] Task request - type:" << static_cast<int>(type)
             << "path:" << path << "silent:" << silent;

    if (path.isEmpty()) {
        fmWarning() << "[TaskManager::startTask] Cannot start task - path is empty";
        return false;
    }

    if (!IndexUtility::isDefaultIndexedDirectory(path)) {
        fmWarning() << "[TaskManager::startTask] Invalid path:" << path;
        return false;
    }

    if (hasRunningTask()) {
        fmWarning() << "[TaskManager::startTask] Task already running, rejecting new task - path:" << path;
        return false;
    }

    fmInfo() << "[TaskManager::startTask] Starting new task - path:" << path
             << "type:" << static_cast<int>(type) << "silent:" << silent;

    TaskHandler handler = getTaskHandler(type);
    if (!handler) {
        fmCritical() << "[TaskManager::startTask] Unknown task type:" << static_cast<int>(type);
        return false;
    }

    Q_ASSERT(!currentTask);
    currentTask = new IndexTask(type, path, handler);
    currentTask->setSilent(silent);
    currentTask->moveToThread(&workerThread);

    connect(currentTask, &IndexTask::progressChanged, this, &TaskManager::onTaskProgress, Qt::QueuedConnection);
    connect(currentTask, &IndexTask::finished, this, &TaskManager::onTaskFinished, Qt::QueuedConnection);
    connect(this, &TaskManager::startTaskInThread, currentTask, &IndexTask::start, Qt::QueuedConnection);
    workerThread.start();

    emit startTaskInThread();
    fmInfo() << "[TaskManager::startTask] Task started successfully in worker thread";
    return true;
}

bool TaskManager::startFileListTask(IndexTask::Type type, const QStringList &fileList, bool silent)
{
    fmInfo() << "[TaskManager::startFileListTask] File list task request - type:" << static_cast<int>(type)
             << "files:" << fileList.size() << "silent:" << silent;

    if (fileList.isEmpty()) {
        fmWarning() << "[TaskManager::startFileListTask] Cannot start task - file list is empty";
        return false;
    }

    if (hasRunningTask()) {
        fmWarning() << "[TaskManager::startFileListTask] Task already running, rejecting new task";
        return false;
    }

    fmInfo() << "[TaskManager::startFileListTask] Starting file list task - files:" << fileList.size()
             << "type:" << static_cast<int>(type) << "silent:" << silent;

    TaskHandler handler;
    switch (type) {
    case IndexTask::Type::CreateFileList:
        handler = TaskHandlers::CreateOrUpdateFileListHandler(*m_context, fileList);
        break;
    case IndexTask::Type::UpdateFileList:
        handler = TaskHandlers::CreateOrUpdateFileListHandler(*m_context, fileList);
        break;
    case IndexTask::Type::RemoveFileList:
        handler = TaskHandlers::RemoveFileListHandler(*m_context, fileList);
        break;
    default:
        fmCritical() << "[TaskManager::startFileListTask] Unknown file list task type:" << static_cast<int>(type);
        return false;
    }

    QString pathId = QString("FileList-%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss"));

    Q_ASSERT(!currentTask);
    currentTask = new IndexTask(type, pathId, handler);
    currentTask->setSilent(silent);
    currentTask->moveToThread(&workerThread);

    connect(currentTask, &IndexTask::progressChanged, this, &TaskManager::onTaskProgress, Qt::QueuedConnection);
    connect(currentTask, &IndexTask::finished, this, &TaskManager::onTaskFinished, Qt::QueuedConnection);
    connect(this, &TaskManager::startTaskInThread, currentTask, &IndexTask::start, Qt::QueuedConnection);
    workerThread.start();

    emit startTaskInThread();
    fmDebug() << "[TaskManager::startFileListTask] File list task started successfully in worker thread";
    return true;
}

bool TaskManager::startFileMoveTask(const QHash<QString, QString> &movedFiles, bool silent)
{
    fmInfo() << "[TaskManager::startFileMoveTask] File move task request - moves:" << movedFiles.size()
             << "silent:" << silent;

    if (movedFiles.isEmpty()) {
        fmWarning() << "[TaskManager::startFileMoveTask] Cannot start task - moved files list is empty";
        return false;
    }

    if (hasRunningTask()) {
        fmWarning() << "[TaskManager::startFileMoveTask] Task already running, rejecting new task";
        return false;
    }

    fmInfo() << "[TaskManager::startFileMoveTask] Starting file move task - moves:" << movedFiles.size()
             << "silent:" << silent;

    TaskHandler handler = TaskHandlers::MoveFileListHandler(*m_context, movedFiles);
    if (!handler) {
        fmCritical() << "[TaskManager::startFileMoveTask] Failed to create move file list handler";
        return false;
    }

    QString pathId = QString("MoveList-%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss"));

    Q_ASSERT(!currentTask);
    currentTask = new IndexTask(IndexTask::Type::MoveFileList, pathId, handler);
    currentTask->setSilent(silent);
    currentTask->moveToThread(&workerThread);

    connect(currentTask, &IndexTask::progressChanged, this, &TaskManager::onTaskProgress, Qt::QueuedConnection);
    connect(currentTask, &IndexTask::finished, this, &TaskManager::onTaskFinished, Qt::QueuedConnection);
    connect(this, &TaskManager::startTaskInThread, currentTask, &IndexTask::start, Qt::QueuedConnection);
    workerThread.start();

    emit startTaskInThread();
    fmDebug() << "[TaskManager::startFileMoveTask] File move task started successfully in worker thread";
    return true;
}

TaskHandler TaskManager::getTaskHandler(IndexTask::Type type)
{
    if (!m_context)
        return nullptr;

    switch (type) {
    case IndexTask::Type::Create:
        return TaskHandlers::CreateIndexHandler(*m_context);
    case IndexTask::Type::Update:
        return TaskHandlers::UpdateIndexHandler(*m_context);
    default:
        fmWarning() << "[TaskManager::getTaskHandler] Unknown task type:" << static_cast<int>(type);
        return nullptr;
    }
}

QString TaskManager::typeToString(IndexTask::Type type)
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

void TaskManager::onTaskProgress(IndexTask::Type type, qint64 count, qint64 total)
{
    if (!currentTask) {
        fmWarning() << "[TaskManager::onTaskProgress] Received progress update but no current task exists";
        return;
    }

    emit taskProgressChanged(typeToString(type), currentTask->taskPath(), count, total);
}

void TaskManager::onTaskFinished(IndexTask::Type type, HandlerResult result)
{
    if (!currentTask) {
        fmWarning() << "[TaskManager::onTaskFinished] Received task finished signal but no current task exists";
        return;
    }

    QString taskPath = currentTask->taskPath();
    fmInfo() << "[TaskManager::onTaskFinished] Task finished - type:" << static_cast<int>(type)
             << "path:" << taskPath << "success:" << result.success << "interrupted:" << result.interrupted;

    emit taskFinished(typeToString(type), taskPath, result.success, result.interrupted, result.useAnything, result.fatal);
    cleanupTask();
}

bool TaskManager::hasRunningTask() const
{
    return currentTask && currentTask->isRunning();
}

void TaskManager::stopCurrentTask()
{
    if (currentTask) {
        fmInfo() << "[TaskManager::stopCurrentTask] Stopping current task - type:" << static_cast<int>(currentTask->taskType())
                 << "path:" << currentTask->taskPath();
        currentTask->stop();
    } else {
        fmDebug() << "[TaskManager::stopCurrentTask] No current task to stop";
    }
}

std::optional<IndexTask::Type> TaskManager::currentTaskType() const
{
    if (!hasRunningTask()) {
        return std::nullopt;
    }

    return currentTask->taskType();
}

std::optional<QString> TaskManager::currentTaskPath() const
{
    if (!hasRunningTask()) {
        return std::nullopt;
    }

    return currentTask->taskPath();
}

void TaskManager::cleanupTask()
{
    if (currentTask) {
        fmDebug() << "[TaskManager::cleanupTask] Cleaning up task resources - type:" << static_cast<int>(currentTask->taskType())
                  << "path:" << currentTask->taskPath();
        disconnect(this, &TaskManager::startTaskInThread, currentTask, &IndexTask::start);
        currentTask->deleteLater();
        currentTask = nullptr;
        fmDebug() << "[TaskManager::cleanupTask] Task cleanup completed";
    }
}

bool TaskManager::isFullScanTask(IndexTask::Type type) const
{
    return type == IndexTask::Type::Create || type == IndexTask::Type::Update;
}
