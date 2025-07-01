// SPDX-FileCopyrightText: 2022 - 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dfmextmenucache.h"

#include <QThread>
#include <QApplication>

namespace dfmplugin_utils {

DFMExtMenuCache &DFMExtMenuCache::instance()
{
    Q_ASSERT(qApp->thread() == QThread::currentThread());
    // 静态变量再堆上分配，最后析构不会有顺序问题
    static DFMExtMenuCache *ins = new DFMExtMenuCache;
    return *ins;
}

DFMExtMenuCache::DFMExtMenuCache()
{
}

}
