// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sidebarfilewatcher.h"
#include "dfm-base/dfm_log_defines.h"

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
        auto watcher = WatcherFactory::create<AbstractFileWatcher>(url, false);
        connect(watcher.data(), &AbstractFileWatcher::subfileCreated, this, &SidebarFileWatcher::onSubfileCreated);
        connect(watcher.data(), &AbstractFileWatcher::fileDeleted, this, &SidebarFileWatcher::onFileDeleted);
        connect(watcher.data(), &AbstractFileWatcher::fileRename, this, &SidebarFileWatcher::onFileRename);
        connect(watcher.data(), &AbstractFileWatcher::fileAttributeChanged, this, &SidebarFileWatcher::onFileAttributeChanged);

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

    auto info = InfoFactory::create<FileInfo>(url, dfmbase::Global::kCreateFileInfoSync);
    if (info && !info->isAttributes(FileInfo::FileIsType::kIsDir))
        return;

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

void SidebarFileWatcher::onFileAttributeChanged(const QUrl &url)
{
    // 1. 读取当前的隐藏文件显示开关状态
    bool showHiddenFiles = Application::genericAttribute(Application::kShowedHiddenFiles).toBool();

    // 2. 创建文件信息对象获取文件属性
    auto info = InfoFactory::create<FileInfo>(url, dfmbase::Global::kCreateFileInfoSync);
    if (!info) {
        fmWarning() << "Failed to create FileInfo for" << url;
        return;
    }

    // 3. 只处理目录，因为侧边栏只显示目录
    if (!info->isAttributes(FileInfo::FileIsType::kIsDir)) {
        return;
    }

    // 4. 判断文件是否为隐藏文件
    bool isHidden = info->isAttributes(FileInfo::FileIsType::kIsHidden);

    // 5. 获取父目录URL用于信号发送
    QUrl parentUrl = url.adjusted(QUrl::RemoveFilename);

    // 6. 根据隐藏文件显示状态和文件隐藏状态决定操作
    if (!showHiddenFiles) {
        if (isHidden) {
            // 如果当前不支持显示隐藏文件，且文件属性变为隐藏，则从侧边栏中移除该条目
            emit directoryRemoved(parentUrl, url);
        } else {
            // 如果当前不支持显示隐藏文件，且文件属性变为非隐藏，则往侧边栏添加条目
            emit directoryCreated(parentUrl, url);
        }
    }
    // 如果显示隐藏文件，则不需要处理，因为不管隐藏与否都会显示
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
