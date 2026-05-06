// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TASKMANAGER_H
#define TASKMANAGER_H

#include "textindex_creator_global.h"
#include "core/indexcontext.h"
#include "indextask.h"

#include <QObject>
#include <QThread>
#include <QHash>

TEXTINDEX_CREATOR_BEGIN_NAMESPACE

class TaskManager : public QObject
{
    Q_OBJECT
public:
    explicit TaskManager(const IndexContext *context, QObject *parent = nullptr);
    ~TaskManager();

    bool startTask(IndexTask::Type type, const QString &path, bool silent = false);

    bool startFileListTask(IndexTask::Type type, const QStringList &fileList, bool silent = false);

    bool startFileMoveTask(const QHash<QString, QString> &movedFiles, bool silent = false);

    bool hasRunningTask() const;
    void stopCurrentTask();

    std::optional<IndexTask::Type> currentTaskType() const;
    std::optional<QString> currentTaskPath() const;

Q_SIGNALS:
    void taskFinished(const QString &type, const QString &path, bool success,
                      bool interrupted, bool useAnything, bool fatal);
    void taskProgressChanged(const QString &type, const QString &path, qint64 count, qint64 total);
    void startTaskInThread();

private Q_SLOTS:
    void onTaskProgress(IndexTask::Type type, qint64 count, qint64 total);
    void onTaskFinished(IndexTask::Type type, TEXTINDEX_CREATOR_NAMESPACE::HandlerResult result);

private:
    void cleanupTask();
    TaskHandler getTaskHandler(IndexTask::Type type);
    bool isFullScanTask(IndexTask::Type type) const;

    const IndexContext *m_context { nullptr };
    QThread workerThread;
    IndexTask *currentTask { nullptr };

    static QString typeToString(IndexTask::Type type);
};

TEXTINDEX_CREATOR_END_NAMESPACE
#endif   // TASKMANAGER_H
