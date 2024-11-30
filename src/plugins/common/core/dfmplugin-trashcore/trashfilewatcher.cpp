// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "trashfilewatcher.h"
#include "private/trashfilewatcher_p.h"
#include "events/trashcoreeventsender.h"

#include <dfm-base/base/schemefactory.h>
#include <dfm-base/base/urlroute.h>
#include <dfm-base/utils/fileutils.h>

#include <dfm-io/dwatcher.h>

#include <QEvent>
#include <QDir>
#include <QDebug>
#include <QApplication>

DFMBASE_USE_NAMESPACE
namespace dfmplugin_trashcore {

TrashFileWatcherPrivate::TrashFileWatcherPrivate(const QUrl &fileUrl, TrashFileWatcher *qq)
    : AbstractFileWatcherPrivate(fileUrl, qq)
{
}

bool TrashFileWatcherPrivate::start()
{
    if (url == FileUtils::trashRootUrl())
        return true;
    if (watcher.isNull())
        return false;
    started = watcher->start();
    if (!started)
        fmWarning() << "watcher start failed, error: " << watcher->lastError().errorMsg();
    return started;
}

bool TrashFileWatcherPrivate::stop()
{
    if (url == FileUtils::trashRootUrl())
        return true;

    if (watcher.isNull())
        return false;
    started = watcher->stop();
    return started;
}

void TrashFileWatcherPrivate::initFileWatcher()
{
    const QUrl &trashUrl = url;
    if (url == FileUtils::trashRootUrl())
        return;

    watcher.reset(new DWatcher(trashUrl));
    if (!watcher) {
        fmWarning("watcher create failed.");
        abort();
    }
    watcher->setWatchType(DWatcher::WatchType::kDir);
}

void TrashFileWatcherPrivate::initConnect()
{
    if (url == FileUtils::trashRootUrl()) {
        auto rootWatcher = TrashCoreEventSender::instance()->trashRootWatcher();
        rootWatcher->disconnect(q);
        connect(rootWatcher.data(), &AbstractFileWatcher::fileAttributeChanged, q, [&](const QUrl &url) {
            emit q->fileAttributeChanged(FileUtils::bindUrlTransform(url));
        });
        connect(rootWatcher.data(), &AbstractFileWatcher::fileDeleted, q, [&](const QUrl &url) {
            emit q->fileDeleted(FileUtils::bindUrlTransform(url));
        });
        connect(rootWatcher.data(), &AbstractFileWatcher::subfileCreated, q, [&](const QUrl &url) {
            emit q->subfileCreated(FileUtils::bindUrlTransform(url));
        });
        connect(rootWatcher.data(), &AbstractFileWatcher::fileRename, q, [&](const QUrl &from, const QUrl &to) {
            emit q->fileRename(FileUtils::bindUrlTransform(from), FileUtils::bindUrlTransform(to));
        });
        return;
    }
    connect(watcher.data(), &DWatcher::fileChanged, q, [&](const QUrl &url) {
        emit q->fileAttributeChanged(FileUtils::bindUrlTransform(url));
    });
    connect(watcher.data(), &DWatcher::fileDeleted, q, [&](const QUrl &url) {
        emit q->fileDeleted(FileUtils::bindUrlTransform(url));
    });
    connect(watcher.data(), &DWatcher::fileAdded, q, [&](const QUrl &url) {
        emit q->subfileCreated(FileUtils::bindUrlTransform(url));
    });
    connect(watcher.data(), &DWatcher::fileRenamed, q, [&](const QUrl &from, const QUrl &to) {
        emit q->fileRename(FileUtils::bindUrlTransform(from), FileUtils::bindUrlTransform(to));
    });
}

TrashFileWatcher::TrashFileWatcher(const QUrl &url, QObject *parent)
    : AbstractFileWatcher(new TrashFileWatcherPrivate(url, this), parent)
{
    dptr = static_cast<TrashFileWatcherPrivate *>(d.data());
    dptr->initFileWatcher();
    dptr->initConnect();
}

TrashFileWatcher::~TrashFileWatcher()
{
}

}
