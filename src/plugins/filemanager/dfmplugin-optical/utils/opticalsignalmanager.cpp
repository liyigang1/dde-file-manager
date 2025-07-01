// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "opticalsignalmanager.h"

using namespace dfmplugin_optical;

OpticalSignalManager *OpticalSignalManager::instance()
{
    // 静态变量再堆上分配，最后析构不会有顺序问题
    static OpticalSignalManager *ins = new OpticalSignalManager;
    return ins;
}

OpticalSignalManager::OpticalSignalManager(QObject *parent)
    : QObject(parent)
{
}
