// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef AUTORELEASEMEMORY_H
#define AUTORELEASEMEMORY_H

#include "service_textindex_global.h"

#include <QObject>

SERVICETEXTINDEX_BEGIN_NAMESPACE

class AutoReleaseMemory : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(AutoReleaseMemory)
public:
    static AutoReleaseMemory *instance();

    ~AutoReleaseMemory() override;

public slots:
    /**
     * @brief releaseMemory 当前使用的内存超出设置的内存就执行内存释放
     */
    void releaseMemory();

private:
    explicit AutoReleaseMemory(QObject *parent = nullptr);

};   // class AutoReleaseMemory

SERVICETEXTINDEX_END_NAMESPACE

#endif   // AUTORELEASEMEMORY_H
