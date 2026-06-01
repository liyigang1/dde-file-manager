// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "trashcoreeventsender.h"
#include "utils/trashcorehelper.h"

#include <dfm-base/dfm_global_defines.h>
#include <dfm-base/base/standardpaths.h>
#include <dfm-base/utils/fileutils.h>
#include <dfm-base/base/schemefactory.h>
#include <dfm-base/file/local/localfilewatcher.h>
#include <dfm-base/interfaces/abstractfilewatcher.h>

#include <dfm-framework/dpf.h>

#include <QDebug>
#include <QUrl>

#include <functional>

using namespace dfmplugin_trashcore;
DFMBASE_USE_NAMESPACE

TrashCoreEventSender::TrashCoreEventSender(QObject *parent)
    : QObject(parent)
{
    auto info = InfoFactory::create<FileInfo>(FileUtils::trashRootUrl());
    auto func = [this, info](bool ok, void *data){
        Q_UNUSED(data);
        if (!ok)
            return;
        isTrashEmpty.store(info->countChildFile() == 0, std::memory_order_release);
    };
    if (!info.isNull())
        info->initQuerierAsync(0, func);
    initTrashWatcher();
}

void TrashCoreEventSender::initTrashWatcher()
{
    trashFileWatcher.reset(new LocalFileWatcher(FileUtils::trashRootUrl(), this));

    connect(trashFileWatcher.data(), &AbstractFileWatcher::subfileCreated, this, &TrashCoreEventSender::sendTrashStateChangedAdd);
    connect(trashFileWatcher.data(), &AbstractFileWatcher::fileDeleted, this, &TrashCoreEventSender::sendTrashStateChangedDel);
    trashFileWatcher->startWatcher();
}

void TrashCoreEventSender::updateTrashStateAsync()
{
    auto info = InfoFactory::create<FileInfo>(FileUtils::trashRootUrl());
    if (info.isNull())
        return;

    auto func = [this, info](bool ok, void *data){
        Q_UNUSED(data);
        if (!ok)
            return;

        const bool empty = info->countChildFile() == 0;
        if (empty == isTrashEmpty.load(std::memory_order_acquire))
            return;

        isTrashEmpty.store(empty, std::memory_order_release);

        dpfSignalDispatcher->publish("dfmplugin_trashcore", "signal_TrashCore_TrashStateChanged", empty);
    };
    info->initQuerierAsync(0, func);
}

TrashCoreEventSender *TrashCoreEventSender::instance()
{
    // 静态变量再堆上分配，最后析构不会有顺序问题
    static TrashCoreEventSender *sender = new TrashCoreEventSender;
    return sender;
}

QSharedPointer<AbstractFileWatcher> TrashCoreEventSender::trashRootWatcher() const
{
    return trashFileWatcher;
}

void TrashCoreEventSender::sendTrashStateChangedDel()
{
    updateTrashStateAsync();
}

void TrashCoreEventSender::sendTrashStateChangedAdd()
{
    updateTrashStateAsync();
}
