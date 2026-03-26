// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "thumbnailfactory.h"
#include "thumbnailcreators.h"
#include "thumbnailhelper.h"

#include <dfm-base/base/schemefactory.h>
#include <dfm-base/utils/universalutils.h>
#include <dfm-base/utils/fileutils.h>
#include <dfm-base/base/device/deviceproxymanager.h>

#include <QGuiApplication>

using namespace dfmbase;
DFMGLOBAL_USE_NAMESPACE

static constexpr int kMaxCountLimit { 50 };
static constexpr int kPushInterval { 100 };   // ms

ThumbnailFactory::ThumbnailFactory(QObject *parent)
    : QObject(parent),
      thread(new QThread),
      worker(new ThumbnailWorker)
{
    registerThumbnailCreator(Mime::kTypeImageVDjvu, ThumbnailCreators::djvuThumbnailCreator);
    registerThumbnailCreator(Mime::kTypeImageVDMultipage, ThumbnailCreators::djvuThumbnailCreator);
    registerThumbnailCreator(Mime::kTypeTextPlain, ThumbnailCreators::textThumbnailCreator);
    registerThumbnailCreator(Mime::kTypeAppPdf, ThumbnailCreators::pdfThumbnailCreator);
    registerThumbnailCreator(Mime::kTypeAppVRRMedia, ThumbnailCreators::videoThumbnailCreatorFfmpeg);
    registerThumbnailCreator("image/*", ThumbnailCreators::imageThumbnailCreator);
    registerThumbnailCreator("audio/*", ThumbnailCreators::audioThumbnailCreator);
    registerThumbnailCreator("video/*", ThumbnailCreators::videoThumbnailCreator);
    registerThumbnailCreator(Mime::kTypeAppAppimage, ThumbnailCreators::appimageThumbnailCreator);
    init();
}

ThumbnailFactory::~ThumbnailFactory()
{
    // 在aboutToQuit信号中已经处理了资源清理
    // 这里只是双重保险，确保线程已经停止
    qCWarning(logDFMBase) << "Thumbnail thread still running in destructor, forcing cleanup";
    onAboutToQuit();
}

void ThumbnailFactory::init()
{
    Q_ASSERT(qApp->thread() == QThread::currentThread());

    taskPushTimer.setSingleShot(true);
    taskPushTimer.setInterval(kPushInterval);
    connect(&taskPushTimer, &QTimer::timeout, this, &ThumbnailFactory::pushTask);
    connect(this, &ThumbnailFactory::thumbnailJob, this, &ThumbnailFactory::doJoinThumbnailJob, Qt::QueuedConnection);
    connect(qApp, &QGuiApplication::aboutToQuit, this, &ThumbnailFactory::onAboutToQuit);

    connect(this, &ThumbnailFactory::addTask, worker.data(), &ThumbnailWorker::onTaskAdded, Qt::QueuedConnection);
    connect(worker.data(), &ThumbnailWorker::thumbnailCreateFinished, this, &ThumbnailFactory::produceFinished, Qt::QueuedConnection);
    connect(worker.data(), &ThumbnailWorker::thumbnailCreateFailed, this, &ThumbnailFactory::produceFailed, Qt::QueuedConnection);

    worker->moveToThread(thread.data());
    thread->start();
}

void ThumbnailFactory::joinThumbnailJob(const QUrl &url, ThumbnailSize size)
{
    if (QThread::currentThread() != qApp->thread()) {
        emit thumbnailJob(url, size);
        return;
    }
    doJoinThumbnailJob(url, size);
}

bool ThumbnailFactory::registerThumbnailCreator(const QString &mimeType, ThumbnailCreator creator)
{
    Q_ASSERT(creator);
    return worker->registerCreator(mimeType, creator);
}

void ThumbnailFactory::onAboutToQuit()
{
    qCInfo(logDFMBase) << "ThumbnailFactory::onAboutToQuit run thread quit!";
    if (worker) {
        worker->stop();
        disconnect(worker.data(), nullptr, nullptr, nullptr);
    }

    // 清除所有待处理的任务
    taskMap.clear();
    taskPushTimer.stop();

    // 断开所有连接，防止信号触发已销毁对象的槽
    disconnect(this, nullptr, nullptr, nullptr);
    disconnect();

    // 同步等待线程结束，超时后强制终止
    if (thread && thread->isRunning()) {
        thread->quit();
        if (!thread->wait(3000)) {
            qCWarning(logDFMBase) << "Thumbnail thread did not stop gracefully, forcing termination";
            thread->terminate();
            thread->wait(1000);
        }
    }
}

void ThumbnailFactory::pushTask()
{
    auto map = std::move(taskMap);
    emit addTask(map);
}

void ThumbnailFactory::doJoinThumbnailJob(const QUrl &url, ThumbnailSize size)
{
    if (FileUtils::containsCopyingFileUrl(url))
        return;

    if (taskMap.contains(url))
        return;

    if (taskMap.isEmpty())
        taskPushTimer.start();


    taskMap.insert(url, size);
    if (taskMap.size() < kMaxCountLimit)
        return;

    pushTask();
}
