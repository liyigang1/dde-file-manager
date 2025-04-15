// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sidebarfilewatcher.h"

#include <dfm-base/base/schemefactory.h>
#include <dfm-base/utils/traversaldirthread.h>

#include <QDebug>

DPSIDEBAR_USE_NAMESPACE
DFMBASE_USE_NAMESPACE

SidebarFileWatcher::SidebarFileWatcher(QObject *parent)
    : QObject(parent)
{
    connect(Application::instance(), &Application::showedHiddenFilesChanged, this, &SidebarFileWatcher::onHiddenFileStatusChanged);
}

SidebarFileWatcher::~SidebarFileWatcher()
{
    stopAllWatchers();
}

void SidebarFileWatcher::watchDirectory(const QUrl &url)
{
    if (url.isValid() && !watchers.contains(url)) {
        auto watcher = WatcherFactory::create<AbstractFileWatcher>(url);
        connect(watcher.data(), &AbstractFileWatcher::subfileCreated, this, &SidebarFileWatcher::onSubfileCreated);
        connect(watcher.data(), &AbstractFileWatcher::fileDeleted, this, &SidebarFileWatcher::onFileDeleted);
        connect(watcher.data(), &AbstractFileWatcher::fileRename, this, &SidebarFileWatcher::onFileRename);
        watcher->startWatcher();
        watchers.insert(url, watcher);
    }
}

void SidebarFileWatcher::unwatchDirectory(const QUrl &url)
{
    if (watchers.contains(url)) {
        auto watcher = watchers.take(url);
        watcher->stopWatcher();
    }
}

void SidebarFileWatcher::stopAllWatchers()
{
    for (auto watcher : watchers) {
        watcher->stopWatcher();
    }
    watchers.clear();
}

void SidebarFileWatcher::onSubfileCreated(const QUrl &url)
{
    QUrl parentUrl = url.adjusted(QUrl::RemoveFilename);
    if (parentUrl.scheme() == Global::Scheme::kBurn && parentUrl.path().contains("/staging_files/")) {
        auto path = parentUrl.path();
        path.replace("/staging_files/", "/disc_files/");
        parentUrl.setPath(path);
    }
    emit directoryCreated(parentUrl, url);
}

void SidebarFileWatcher::onFileDeleted(const QUrl &url)
{
    // 获取父目录的 URL
    QUrl parentUrl = url.adjusted(QUrl::RemoveFilename);
    emit directoryRemoved(parentUrl, url);
    if (watchers.contains(url)) {
        auto w = watchers.take(url);
        w->stopWatcher();
    }
}

void SidebarFileWatcher::onFileRename(const QUrl &oldUrl, const QUrl &newUrl)
{
    // 检查新路径是否是目录
    QFileInfo newFileInfo(newUrl.toLocalFile());

    if (newFileInfo.isDir()) {
        // 获取父目录的 URL
        QUrl parentUrl = newUrl.adjusted(QUrl::RemoveFilename);
        emit directoryRenamed(parentUrl, oldUrl, newUrl);
    }
}

void SidebarFileWatcher::onHiddenFileStatusChanged(bool showHidden)
{
    auto watchingUrls = watchers.keys();
    for (auto url : watchingUrls) {
        TraversalDirThread *t = new TraversalDirThread(url, {}, QDir::Hidden | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::FollowSymlinks);
        connect(t, &TraversalDirThread::updateChildren, this, [=](const QList<QUrl> &dirs) {
            setDirsVisible(showHidden, dirs);
        });
        connect(t, &TraversalDirThread::finished, t, &TraversalDirThread::deleteLater);
        t->start();
    }
}

void SidebarFileWatcher::setDirsVisible(bool showHidden, const QList<QUrl> &dirs)
{
    if (dirs.isEmpty())
        return;
    auto parentUrl = dirs.first().adjusted(QUrl::RemoveFilename);
    for (auto dir : dirs) {
        auto info = InfoFactory::create<FileInfo>(dir, dfmbase::Global::kCreateFileInfoSync);
        if (!info || !info->isAttributes(FileInfo::FileIsType::kIsHidden))
            continue;
        if (showHidden)
            emit directoryCreated(parentUrl, dir);
        else
            emit directoryRemoved(parentUrl, dir);
    }
}
