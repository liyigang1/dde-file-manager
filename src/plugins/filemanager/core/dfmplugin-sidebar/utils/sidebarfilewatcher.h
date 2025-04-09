// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SIDEBARFILEWATCHER_H
#define SIDEBARFILEWATCHER_H

#include "dfmplugin_sidebar_global.h"

#include <dfm-base/file/local/localfilewatcher.h>

#include <QObject>
#include <QMap>
#include <QSet>
#include <QUrl>

DPSIDEBAR_BEGIN_NAMESPACE

class SidebarFileWatcher : public QObject
{
    Q_OBJECT
public:
    explicit SidebarFileWatcher(QObject *parent = nullptr);
    ~SidebarFileWatcher();

    void watchDirectory(const QUrl &url);
    void unwatchDirectory(const QUrl &url);
    void stopAllWatchers();

signals:
    void directoryCreated(const QUrl &parentUrl, const QUrl &url);
    void directoryRemoved(const QUrl &parentUrl, const QUrl &url);
    void directoryRenamed(const QUrl &parentUrl, const QUrl &oldUrl, const QUrl &newUrl);

private slots:
    void onSubfileCreated(const QUrl &url);
    void onFileDeleted(const QUrl &url);
    void onFileRename(const QUrl &oldUrl, const QUrl &newUrl);

private:
    QMap<QUrl, DFMBASE_NAMESPACE::LocalFileWatcher*> watchers;
};

DPSIDEBAR_END_NAMESPACE

#endif   // SIDEBARFILEWATCHER_H
