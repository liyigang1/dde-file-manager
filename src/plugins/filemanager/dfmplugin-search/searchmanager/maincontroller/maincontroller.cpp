// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "maincontroller.h"
#include "searchmanager/searcher/fulltext/fulltextsearcher.h"

#include <dfm-base/base/application/settings.h>
#include <dfm-base/base/application/application.h>
#include <dfm-base/dfm_global_defines.h>
#include <dfm-base/file/local/localfilehandler.h>
#include <dfm-base/base/schemefactory.h>

#include <QApplication>
#include <QtConcurrent>
#include <QUrl>
#include <QDir>
#include <QDebug>
#include <QProcess>

#include <dfm-io/dfile.h>

DFMBASE_USE_NAMESPACE
DPSEARCH_USE_NAMESPACE

MainController::MainController(QObject *parent)
    : QObject(parent)
{
}

MainController::~MainController()
{
    for (auto &task : taskManager) {
        task->stop();
        task->deleteSelf();
        task = nullptr;
    }
    taskManager.clear();
}

void MainController::stop(QString taskId)
{
    if (taskManager.contains(taskId)) {
        disconnect(taskManager[taskId]);
        taskManager[taskId]->stop();
        taskManager[taskId]->deleteSelf();
        taskManager[taskId] = nullptr;
        taskManager.remove(taskId);
    }
}

bool MainController::doSearchTask(QString taskId, const QUrl &url, const QString &keyword)
{
    if (taskManager.contains(taskId))
        stop(taskId);

    auto task = new TaskCommander(taskId, url, keyword);
    Q_ASSERT(task);
    fmInfo() << "new task: " << task << task->taskID();

    //直连，防止1被事件循环打乱时序
    connect(task, &TaskCommander::matched, this, &MainController::matched, Qt::DirectConnection);
    connect(task, &TaskCommander::finished, this, &MainController::onFinished, Qt::DirectConnection);

    if (task->start()) {
        taskManager.insert(taskId, task);
        return true;
    }

    fmWarning() << "fail to start task " << task << task->taskID();
    task->deleteSelf();
    return false;
}

QList<QUrl> MainController::getResults(QString taskId)
{
    if (taskManager.contains(taskId))
        return taskManager[taskId]->getResults();

    return {};
}

void MainController::onFinished(QString taskId)
{
    if (taskManager.contains(taskId))
        stop(taskId);

    emit searchCompleted(taskId);
}

void MainController::onIndexFullTextSearchChanged(bool enable)
{
    // enable 检查是否有分词修改的标志文件
    FullTextSearcher searcher(QUrl(), "");
    if (!searcher.indexExists() || !enable)
        return;
    auto participlePath = searcher.indexFolderPath() + "/participle.Lock";
    QUrl participleUrl;
    participleUrl.setHost("");
    participleUrl.setScheme(dfmbase::Global::Scheme::kFile);
    participleUrl.setPath(participlePath);
    if (dfmio::DFile(participleUrl).exists())
        return;

    // 删除当前的索引
    auto indexDir = participleUrl;
    indexDir.setPath(searcher.indexFolderPath());
    LocalFileHandler handler;
    auto it = DirIteratorFactory::create<AbstractDirIterator>(indexDir, QStringList(),
                                                              QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    while (it->hasNext()) {
        auto url  = it->next();
        if (url.isValid())
            handler.deleteFile(url);
    }

    // 写入分词文件
    handler.touchFile(participleUrl);
}
