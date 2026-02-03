// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "autoreleasememory.h"
#include "utils/textindexconfig.h"

#include "dfm-base/utils/sysinfoutils.h"

#include <unistd.h>
#include <fcntl.h>
#include <mutex>
#include <malloc.h>

SERVICETEXTINDEX_BEGIN_NAMESPACE

AutoReleaseMemory::AutoReleaseMemory(QObject *parent) : QObject(parent)
{

}

AutoReleaseMemory *AutoReleaseMemory::instance()
{
    static AutoReleaseMemory instance;
    return &instance;
}

AutoReleaseMemory::~AutoReleaseMemory()
{
    disconnect();
}

void AutoReleaseMemory::releaseMemory()
{
    // 大于设定值就释放内存
    float memUsage = dfmbase::SysInfoUtils::getMemoryUsage(getpid());
    if (memUsage > TextIndexConfig::instance().maxMemoryToRelease())
        malloc_trim(0);
}

SERVICETEXTINDEX_END_NAMESPACE
