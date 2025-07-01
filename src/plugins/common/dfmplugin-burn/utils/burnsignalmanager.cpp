// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "burnsignalmanager.h"

DPBURN_USE_NAMESPACE

BurnSignalManager *BurnSignalManager::instance()
{
    // 静态变量再堆上分配，最后析构不会有顺序问题
    static BurnSignalManager *ins = new  BurnSignalManager;
    return ins;
}

BurnSignalManager::BurnSignalManager(QObject *parent)
    : QObject(parent)
{
}
