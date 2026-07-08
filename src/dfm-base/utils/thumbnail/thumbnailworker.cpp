// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "thumbnailworker.h"
#include "private/thumbnailworker_p.h"
#include "thumbnailcreators.h"

#include <dfm-base/utils/universalutils.h>
#include <dfm-base/utils/fileutils.h>
#include <dfm-base/base/schemefactory.h>
#include <dfm-base/base/urlroute.h>

#include <QtConcurrent>
#include <QPainter>
#include <QDebug>

using namespace dfmbase;

ThumbnailWorkerPrivate::ThumbnailWorkerPrivate(ThumbnailWorker *qq)
    : q(qq)
{
    thumbHelper.initSizeLimit();
}

QString ThumbnailWorkerPrivate::createThumbnail(const FileInfoPointer &info, Global::ThumbnailSize size)
{
    if (isStoped)
        return "";

    if (info.isNull()) {
        qWarning(logDFMBase()) << " ThumbnailWorkerPrivate::createThumbnail 72 info is nullptr.";
        return "";
    }

    if (!thumbHelper.canGenerateThumbnail(info)) {
        qCDebug(logDFMBase) << "ThumbnailWorkerPrivate::createThumbnail 72: the file does not support generate thumbnails: " << info->fileUrl();
        return "";
    }

    const auto &absoluteFilePath = info->pathOf(PathInfoType::kAbsoluteFilePath);
    if (thumbHelper.defaultThumbnailDirs().contains(info->pathOf(PathInfoType::kAbsolutePath)))
        return absoluteFilePath;

    QImage img;
    const auto &mime = mimeDb.mimeTypeForFile(info);
    const auto &mimeName = mime.name();

    ThumbnailWorker::ThumbnailCreator creator { nullptr };
    {
        QMutexLocker lk(&creatorMutex);
        creator = creators.value(mimeName);

        if (!creator) {   // pattern match
            for (auto &mimeRegx : creators.keys()) {
                if (isStoped)
                    return "";

                QRegularExpression regx(mimeRegx);
                if (mimeName.contains(regx)) {
                    creator = creators.value(mimeRegx);
                    break;
                }
            }
        }
    }

    if (creator)
        img = creator(info, size, &isStoped);

    // default image generator if cannot create by customized function
    if (img.isNull())
        img = ThumbnailCreators::defaultThumbnailCreator(info, size, &isStoped);

    if (img.isNull()) {
        qCWarning(logDFMBase) << "thumbnail: cannot generate thumbnail for file: " << info->fileUrl();
        return "";
    }

    if (img.height() > size || img.width() > size)
        img = img.scaled({ size, size }, Qt::KeepAspectRatio);

    if (isStoped)
        return "";

    return thumbHelper.saveThumbnail(info, img, size);
}

bool ThumbnailWorkerPrivate::checkFileStable(const FileInfoPointer &info)
{
    if (!info)
        return true;

    // 修改时间稳定
    qint64 mtime = info->timeOf(TimeInfoType::kMetadataChangeTimeSecond).toLongLong();
    qint64 curTime = QDateTime::currentDateTime().toTime_t();
    qint64 diffTime = curTime - mtime;

    if (diffTime < 2 && diffTime >= 0)
        return false;

    return true;
}

void ThumbnailWorkerPrivate::startDelayWork()
{
    if (!delayTimer) {
        delayTimer = new QTimer(q);
        delayTimer->setInterval(2 * 1000);
        delayTimer->setSingleShot(true);
        q->connect(delayTimer, &QTimer::timeout, q, [this] { q->onTaskAdded(delayTaskMap); }, Qt::QueuedConnection);
    }

    delayTimer->start();
}

QUrl ThumbnailWorkerPrivate::setCheckCount(const QUrl &url, int count)
{
    QUrl tmpUrl(url);
    QUrlQuery query(url.query());
    query.removeQueryItem("checkCount");
    query.addQueryItem("checkCount", QString::number(count));
    tmpUrl.setQuery(query);

    return tmpUrl;
}

int ThumbnailWorkerPrivate::checkCount(const QUrl &url)
{
    if (!url.hasQuery())
        return 0;

    QUrlQuery query(url.query());
    return query.queryItemValue("checkCount").toInt();
}

QUrl ThumbnailWorkerPrivate::clearCheckCount(const QUrl &url)
{
    if (!url.hasQuery())
        return url;

    QUrl tmpUrl(url);
    QUrlQuery query(url.query());
    query.removeQueryItem("checkCount");
    tmpUrl.setQuery(query);

    return tmpUrl;
}

ThumbnailWorker::ThumbnailWorker(QObject *parent)
    : QObject(parent),
      d(new ThumbnailWorkerPrivate(this))
{
}

ThumbnailWorker::~ThumbnailWorker()
{
    QMutexLocker lk(&d->creatorMutex);
    d->creators.clear();
}

bool ThumbnailWorker::registerCreator(const QString &mimeType, ThumbnailWorker::ThumbnailCreator creator)
{
    Q_ASSERT(creator);

    QMutexLocker lk(&d->creatorMutex);
    if (d->creators.contains(mimeType)) {
        qCWarning(logDFMBase) << "register failed, the mime type has already been registered." << mimeType;
        return false;
    }

    d->creators.insert(mimeType, creator);
    return true;
}

void ThumbnailWorker::stop()
{
    qCDebug(logDFMBase()) << "ThumbnailWorker::stop stop worker!!";
    disconnect();
    d->isStoped = true;
    if (d->delayTimer) {
        d->delayTimer->stop();
    }
    {
        QMutexLocker lk(&d->creatorMutex);
        d->creators.clear();
    }
}

void ThumbnailWorker::onTaskAdded(const ThumbnailTaskMap &taskMap)
{
    if (d->isStoped)
        return;

    QMapIterator<QUrl, Global::ThumbnailSize> iter(taskMap);
    while (iter.hasNext()) {
        if (d->isStoped)
            break;

        iter.next();
        QUrl origUrl = iter.key();
        QUrl realUrl = origUrl;
        realUrl.setQuery(QString());
        qCDebug(logDFMBase()) << " ThumbnailWorker::onTaskAdded while " << origUrl
                              << realUrl << QThread::currentThreadId();

        // 检查是否是链接文件，如果是则保存到链接文件而不是目标文件
        QUrl thumbSaveUrl = realUrl;
        QString symlinkTarget = FileUtils::symlinkTarget(realUrl, true);
        if (!symlinkTarget.isEmpty()) {
            qCDebug(logDFMBase) << "File is symlink, thumbnail will be saved to link file:" << realUrl.toString() << "target:" << symlinkTarget;
            // 缩略图保存到链接文件，但生成时使用目标文件
            thumbSaveUrl = realUrl;   // 保存到链接文件
            realUrl = QUrl::fromLocalFile(symlinkTarget);   // 生成时使用目标文件
        }

        auto info = InfoFactory::create<FileInfo>(realUrl, Global::CreateFileInfoType::kCreateFileInfoSync);
        if (!info) {
            qCWarning(logDFMBase) << "ThumbnailWorker::onTaskAdded creat file info error: nullptr, url = " << realUrl;
            continue;
        }
        auto size = iter.value();
        auto func = [this, origUrl, realUrl, thumbSaveUrl, size, info](bool successed, void *data){
            Q_UNUSED(data);
            qCDebug(logDFMBase()) << " ThumbnailWorker::onTaskAdded call backfunc " << origUrl
                                  << realUrl << QThread::currentThreadId();
            if (!successed) {
                qCWarning(logDFMBase) << "ThumbnailWorker::createThumbnail info initQuerierAsync failed, url = "
                                      << realUrl << origUrl;
                return;
            }

            // thumbnailworker.cpp - 跳过目录缩略图生成
            // 在确认 info 有效后，优先判断是否为目录，避免对目录执行无意义的缩略图IO
            if (info->isAttributes(OptInfoType::kIsDir))
                return;

            if (d->isStoped)
                return;

            if (!d->thumbHelper.checkThumbEnable(info))
                return;

            const auto &img = d->thumbHelper.thumbnailImage(info, size);
            if (!img.isNull()) {
                Q_EMIT thumbnailCreateFinished(thumbSaveUrl, img.text(QT_STRINGIFY(Thumb::Path)));
                return;
            }

            createThumbnail(origUrl, info, size, thumbSaveUrl);
        };
        info->initQuerierAsync(0, func);
    }
}

void ThumbnailWorker::createThumbnail(QUrl origUrl, const FileInfoPointer &info, Global::ThumbnailSize size, const QUrl &saveUrl)
{
    QUrl realUrl = info->fileUrl();
    realUrl.setQuery(QString());

    if (d->isStoped)
        return;

    // 如果指定了保存URL，使用它作为缩略图的保存位置
    QUrl finalSaveUrl = saveUrl.isEmpty() ? realUrl : saveUrl;
    if (!d->checkFileStable(info)) {
        if (!d->delayTaskMap.contains(origUrl)) {
            origUrl = d->setCheckCount(origUrl, 1);
        } else {
            d->delayTaskMap.remove(origUrl);
            // 超过10次，放弃生成
            auto count = d->checkCount(origUrl);
            if (++count > 10)
                return;

            origUrl = d->setCheckCount(origUrl, count);
        }

        d->delayTaskMap.insert(origUrl, size);
        d->startDelayWork();
        return;
    }

    // create thumbnail
    const auto &thumbnailPath = d->createThumbnail(info, size);
    if (!thumbnailPath.isEmpty())
        Q_EMIT thumbnailCreateFinished(finalSaveUrl, thumbnailPath);
    else
        Q_EMIT thumbnailCreateFailed(finalSaveUrl);
}
