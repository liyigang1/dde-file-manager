// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "progressnotifier.h"

TEXTINDEX_CREATOR_USE_NAMESPACE

ProgressNotifier *ProgressNotifier::instance()
{
    static ProgressNotifier instance;
    return &instance;
}
