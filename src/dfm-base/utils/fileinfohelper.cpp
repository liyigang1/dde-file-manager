// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fileinfohelper.h"

#include <dfm-base/file/local/asyncfileinfo.h>
#include <dfm-base/utils/fileutils.h>
#include <dfm-base/utils/networkutils.h>
#include <dfm-base/base/device/deviceutils.h>

#include <QGuiApplication>
#include <QTimer>

Q_DECLARE_METATYPE(QSharedPointer<dfmio::DFileInfo>);

DFMBASE_USE_NAMESPACE
FileInfoHelper::FileInfoHelper(QObject *parent)
    : QObject(parent), thread(new QThread), worker(new FileInfoAsycWorker)
{
    moveToThread(qApp->thread());
    init();
}

void FileInfoHelper::init()
{
    // connect quit app to stop
    connect(qApp, &QGuiApplication::aboutToQuit, this, &FileInfoHelper::aboutToQuit);
    // connect thumb

    // connect file info async worker
    connect(this, &FileInfoHelper::fileCount, worker.data(), &FileInfoAsycWorker::fileConutAsync, Qt::QueuedConnection);
    connect(worker.data(), &FileInfoAsycWorker::fileConutAsyncFinish, this, &FileInfoHelper::fileCountFinished, Qt::QueuedConnection);
    connect(this, &FileInfoHelper::fileMimeType, worker.data(), &FileInfoAsycWorker::fileMimeType, Qt::QueuedConnection);
    connect(this, &FileInfoHelper::fileInfoRefresh, worker.data(), &FileInfoAsycWorker::fileRefresh, Qt::QueuedConnection);
    connect(worker.data(), &FileInfoAsycWorker::fileMimeTypeFinished, this, &FileInfoHelper::fileMimeTypeFinished, Qt::QueuedConnection);
    connect(this, &FileInfoHelper::fileRefreshRequest, this, &FileInfoHelper::handleFileRefresh, Qt::QueuedConnection);

    worker->moveToThread(thread.data());
    thread->start();
}

void FileInfoHelper::threadHandleDfmFileInfo(const QSharedPointer<FileInfo> dfileInfo)
{
    if (stoped)
        return;

    auto asyncInfo = dfileInfo.dynamicCast<AsyncFileInfo>();
    if (asyncInfo.isNull())
        return;

    auto resluts = asyncInfo->cacheAsyncAttributes();

    while (resluts == 0) {
        QThread::msleep(5);
        resluts = asyncInfo->cacheAsyncAttributes();
    }

    if (resluts <= 1) {
        checkInfoRefresh(dfileInfo);
        return;
    }

    emit fileRefreshFinished(dfileInfo->fileUrl(), QString::number(quintptr(dfileInfo.data()), 16), false);

    auto notifyUrls = asyncInfo->notifyUrls();
    for (const auto &url : notifyUrls.keys()) {
        for (const auto &strToken : notifyUrls.values(url))
            emit fileRefreshFinished(url, strToken, true);
    }

    checkInfoRefresh(dfileInfo);
}

QSharedPointer<FileInfoHelperUeserData> FileInfoHelper::fileCountAsync(QUrl &url)
{
    if (stoped)
        return nullptr;
    QSharedPointer<FileInfoHelperUeserData> data(new FileInfoHelperUeserData);
    emit fileCount(url, data);
    return data;
}

QSharedPointer<FileInfoHelperUeserData> FileInfoHelper::fileMimeTypeAsync(const QUrl &url, const QMimeDatabase::MatchMode mode, const QString &inod, const bool isGvfs)
{
    if (stoped)
        return nullptr;
    QSharedPointer<FileInfoHelperUeserData> data(new FileInfoHelperUeserData);
    emit fileMimeType(url, mode, inod, isGvfs, data);
    return data;
}

void FileInfoHelper::fileRefreshAsync(const QSharedPointer<FileInfo> dfileInfo)
{
    if (stoped || !dfileInfo)
        return;
    emit fileRefreshRequest(dfileInfo);
}

void FileInfoHelper::cacheFileInfoByThread(const QSharedPointer<FileInfo> dfileInfo)
{
    if (stoped)
        return;
    QTimer::singleShot(10, [this, dfileInfo]() {
        threadHandleDfmFileInfo(dfileInfo);
    });
}

FileInfoHelper::~FileInfoHelper()
{
    aboutToQuit();
}

FileInfoHelper &FileInfoHelper::instance()
{
    // 静态变量再堆上分配，最后析构不会有顺序问题
    static FileInfoHelper *helper = new FileInfoHelper;
    return *helper;
}

void FileInfoHelper::aboutToQuit()
{
    stoped = true;
    thread->quit();
    worker->stopWorker();
    thread->wait(3000);
    pool.waitForDone();
}

void FileInfoHelper::handleFileRefresh(QSharedPointer<FileInfo> dfileInfo)
{
    if (stoped)
        return;

    assert(qApp->thread() == QThread::currentThread());

    if (!dfileInfo)
        return;

    auto asyncInfo = dfileInfo.dynamicCast<AsyncFileInfo>();
    if (!asyncInfo)
        return;

    auto callback = [dfileInfo, asyncInfo, this](bool success, void *data) {
        Q_UNUSED(data);
        if (!success) {
            FileInfoHelper::instance().checkInfoRefresh(dfileInfo);
            if (DeviceUtils::isSamba(dfileInfo->fileUrl())
                    && asyncInfo->errorCodeFromDfmio() == DFMIOErrorCode::DFM_IO_ERROR_HOST_IS_DOWN
                    && !NetworkUtils::instance()->checkFtpOrSmbBusy(dfileInfo->fileUrl())) {
                emit this->smbSeverMayModifyPassword(dfileInfo->fileUrl());
            }
            return;
        }
        FileInfoHelper::instance().cacheFileInfoByThread(dfileInfo);
    };

    if (qureingInfo.containsByLock(dfileInfo) && needQureingInfo.containsByLock(dfileInfo))
        return;

    if (qureingInfo.containsByLock(dfileInfo)) {
        needQureingInfo.appendByLock(dfileInfo);
        return;
    }
    qureingInfo.appendByLock(dfileInfo);
    asyncInfo->asyncQueryDfmFileInfo(0, callback);
}

void FileInfoHelper::checkInfoRefresh(QSharedPointer<FileInfo> dfileInfo)
{
    qureingInfo.removeOneByLock(dfileInfo);
    if (needQureingInfo.containsByLock(dfileInfo)) {
        needQureingInfo.removeOneByLock(dfileInfo);
        fileRefreshAsync(dfileInfo);
    }
}
