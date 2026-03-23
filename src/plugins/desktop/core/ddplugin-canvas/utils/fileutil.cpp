// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fileutil.h"

#include <dfm-base/base/schemefactory.h>
#include <dfm-base/file/local/desktopfileinfo.h>

#include <QReadWriteLock>

DFMBASE_USE_NAMESPACE
using namespace ddplugin_canvas;

class DesktopFileCreatorGlogal : public DesktopFileCreator
{
};
Q_GLOBAL_STATIC(DesktopFileCreatorGlogal, desktopFileCreatorGlogal)

DesktopFileCreator *DesktopFileCreator::instance()
{
    return desktopFileCreatorGlogal;
}

FileInfoPointer DesktopFileCreator::createFileInfo(const QUrl &url, dfmbase::Global::CreateFileInfoType cache)
{
    QString errString;
    auto itemInfo = InfoFactory::create<FileInfo>(url, cache, &errString);
    if (Q_UNLIKELY(!itemInfo)) {
        fmInfo() << "create FileInfo error: " << errString << url;
        return nullptr;
    }

    return itemInfo;
}

DesktopFileCreator::DesktopFileCreator()
{
}

DesktopViewPaintUtilsPrivate::DesktopViewPaintUtilsPrivate(QObject *parent)
    : QObject (parent)
{
    t.setSingleShot(true);
    connect(&t, &QTimer::timeout, this, [this]{
        enabledUpdate.store(true, std::memory_order_acquire);
    });
}

DesktopViewPaintUtilsPrivate::~DesktopViewPaintUtilsPrivate()
{

}

class DesktopViewEnableSetUpdateGlogal : public DesktopViewPaintUtils
{
};
Q_GLOBAL_STATIC(DesktopViewEnableSetUpdateGlogal, desktopViewEnableSetUpdateGlogal)

DesktopViewPaintUtils *DesktopViewPaintUtils::instance()
{
    return desktopViewEnableSetUpdateGlogal;
}

bool DesktopViewPaintUtils::enabledSetUpdate() const
{
    return d->enabledUpdate.load(std::memory_order_release);
}

void DesktopViewPaintUtils::delaySetEnableUpdate(const int time)
{
    if (d->t.isActive())
        return;

    d->t.setSingleShot(true);
    d->t.setInterval(time);
    d->t.start();
}

void DesktopViewPaintUtils::unableUpdate()
{
    d->enabledUpdate.store(false, std::memory_order_acquire);
    d->t.stop();
}

DesktopViewPaintUtils::DesktopViewPaintUtils(QObject *parent)
    : QObject (parent), d(new DesktopViewPaintUtilsPrivate)
{

}
