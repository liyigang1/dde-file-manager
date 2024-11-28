// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DFMPLUGIN_SHRED_GLOBAL_H
#define DFMPLUGIN_SHRED_GLOBAL_H

#include <dfm-base/dfm_log_defines.h>

#define DPSHRED_NAMESPACE dfmplugin_shred
#define DPSHRED_BEGIN_NAMESPACE namespace DPSHRED_NAMESPACE {
#define DPSHRED_END_NAMESPACE }
#define DPSHRED_USE_NAMESPACE using namespace DPSHRED_NAMESPACE;

DPSHRED_BEGIN_NAMESPACE
DFM_LOG_USE_CATEGORY(DPSHRED_NAMESPACE)
DPSHRED_END_NAMESPACE

#endif   // DFMPLUGIN_SHRED_GLOBAL_H
