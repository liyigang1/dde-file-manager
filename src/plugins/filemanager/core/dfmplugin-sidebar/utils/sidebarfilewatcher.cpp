// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sidebarfilewatcher.h"

#include <QFileInfo>
#include <QDebug>

DPSIDEBAR_USE_NAMESPACE

SidebarFileWatcher::SidebarFileWatcher(QObject *parent)
    : QObject(parent)
{
}

SidebarFileWatcher::~SidebarFileWatcher()
{
    stopAllWatchers();
}

void SidebarFileWatcher::watchDirectory(const QUrl &url)
{
    if (url.isValid() && url.isLocalFile() && !watchers.contains(url)) {
        auto watcher = new DFMBASE_NAMESPACE::LocalFileWatcher(url, this);
        connect(watcher, &DFMBASE_NAMESPACE::LocalFileWatcher::subfileCreated, this, &SidebarFileWatcher::onSubfileCreated);
        connect(watcher, &DFMBASE_NAMESPACE::LocalFileWatcher::fileDeleted, this, &SidebarFileWatcher::onFileDeleted);
        connect(watcher, &DFMBASE_NAMESPACE::LocalFileWatcher::fileRename, this, &SidebarFileWatcher::onFileRename);
        watcher->startWatcher();
        watchers.insert(url, watcher);
    }
}

void SidebarFileWatcher::unwatchDirectory(const QUrl &url)
{
    if (watchers.contains(url)) {
        auto watcher = watchers.take(url);
        watcher->stopWatcher();
        watcher->deleteLater();
    }
}

void SidebarFileWatcher::stopAllWatchers()
{
    for (auto watcher : watchers) {
        watcher->stopWatcher();
        watcher->deleteLater();
    }
    watchers.clear();
}

void SidebarFileWatcher::onSubfileCreated(const QUrl &url)
{
    // 检查是否是目录
    QFileInfo fileInfo(url.toLocalFile());
    if (fileInfo.isDir()) {
        // 获取父目录的 URL
        QUrl parentUrl = url.adjusted(QUrl::RemoveFilename);
        emit directoryCreated(parentUrl, url);
    }
}

void SidebarFileWatcher::onFileDeleted(const QUrl &url)
{
    // 获取父目录的 URL
    QUrl parentUrl = url.adjusted(QUrl::RemoveFilename);
    emit directoryRemoved(parentUrl, url);
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
