// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "opticalshare.h"
#include "opticalsharedbus.h"

namespace daemonplugin_opticalshare {
DFM_LOG_REISGER_CATEGORY(DAEMONPOPTICALSHARE_NAMESPACE)

void OpticalShare::initialize()
{
}

bool OpticalShare::start()
{
    mng.reset(new OpticalShareDBus(this));
    return true;
}

}   // namespace daemonplugin_opticalshare
