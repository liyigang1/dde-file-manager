// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "filemonitorplugin.h"
#include "dbusfilemonitorjob.h"

#include <dfm-framework/listener/listener.h>
#include <dfm-base/base/configs/dconfig/dconfigmanager.h>

DPF_USE_NAMESPACE

namespace ddplugin_filemonitor {
DFM_LOG_REISGER_CATEGORY(DDP_FILEMONITOR_NAMESPACE)
inline constexpr char kFileOperations[] { "org.deepin.dde.file-manager.operations" };
inline constexpr char kBroadcastPaste[] { "file.operation.broadcastpastevent" };

void BackgroundPlugin::initialize()
{
    QString err;
    auto ret = dfmbase::DConfigManager::instance()->addConfig("org.deepin.dde.file-manager.operations", &err);
    if (!ret)
        fmWarning() << "create dconfig failed: " << err;

    if (!dfmbase::DConfigManager::instance()->value(kFileOperations, kBroadcastPaste, false).toBool())
        return;

    DbusFileMonitorJob *worker = new DbusFileMonitorJob;
    workThread = new QThread;
    QObject::connect(workThread, &QThread::started,worker,&DbusFileMonitorJob::initDbusConnection);
    QObject::connect(workThread, &QThread::finished, workThread, &QThread::deleteLater);
    QObject::connect(workThread, &QThread::destroyed,workThread,&DbusFileMonitorJob::deleteLater);

    worker->moveToThread(workThread);
}

bool BackgroundPlugin::start()
{
    if (!workThread)
        return true;
    workThread->start();
    return true;
}

void BackgroundPlugin::stop()
{
    if (workThread) {
        workThread->quit();
        workThread->wait();
    }
}

}   // namespace ddplugin_background
