// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SHREDUTILS_H
#define SHREDUTILS_H

#include <QObject>
#include <QWidget>
#include <QPair>
#include <dfmplugin_shred_global.h>
#include <dfm-base/dfm_base_global.h>

namespace dfmplugin_shred {
class ShredUtils : public QObject
{
    Q_OBJECT
public:
    explicit ShredUtils(QObject *parent = nullptr);
    static QPair<QWidget *, QWidget *> createSettingButton(QObject *opt);

    static bool isShredEnabled();
    static void setShredEnabled(bool enable);
    static void initDconfig();

private:
    static void setDConfig();
};

}   // namespace dfmplugin_shred

#endif   // SHREDUTILS_H
