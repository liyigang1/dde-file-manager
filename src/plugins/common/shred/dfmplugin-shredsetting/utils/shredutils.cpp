// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "shredutils.h"

#include <dfm-base/base/configs/dconfig/dconfigmanager.h>

#include <QPushButton>
#include <DSwitchButton>
#include <DSettingsOption>
#include <QDebug>
#include <QVBoxLayout>
#include <DTipLabel>
#include <QProcess>
#include <iostream>

DWIDGET_USE_NAMESPACE
using namespace dfmbase;
namespace dfmplugin_shred {
DFM_LOG_REISGER_CATEGORY(dfmplugin_shred)

const QString kShredDConfigName = "org.deepin.dde.file-manager.shred";

ShredUtils::ShredUtils(QObject *parent)
    : QObject(parent)
{
}

QPair<QWidget *, QWidget *> ShredUtils::createSettingButton(QObject *opt)
{
    auto option = qobject_cast<Dtk::Core::DSettingsOption *>(opt);

    auto widget = new QWidget;
    widget->setContentsMargins(0, 0, 0, 0);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setMargin(0);

    auto lab = new QLabel(option->data("text").toString());
    layout->addWidget(lab);

    QHBoxLayout *hLayout = new QHBoxLayout();
    hLayout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(hLayout);

    auto msgLabel = new DTipLabel(option->data("message").toString(), widget);
    msgLabel->setAlignment(Qt::AlignLeft);
    msgLabel->setWordWrap(true);

    hLayout->addWidget(msgLabel);

    auto btn = new DSwitchButton;
    // 设置初始状态
    bool status = isShredEnabled();
    btn->setChecked(status);
    option->setValue(status);

    // 连接点击信号
    QObject::connect(btn, &DSwitchButton::clicked, option, [option](bool checked) {
        setShredEnabled(checked);
        option->setValue(checked);

        fmDebug() << "Shred function" << checked;
    });

    QObject::connect(option, &Dtk::Core::DSettingsOption::valueChanged, btn, [btn](QVariant value) {
        bool checked = value.toBool();
        fmDebug() << "Shred function DSettingsOption" << checked;
        setShredEnabled(checked);
        btn->setChecked(checked);
    });

    return qMakePair(widget, btn);
}

void ShredUtils::initDconfig()
{
    QString err;
    if (!DConfigManager::instance()->addConfig(kShredDConfigName, &err))
        fmWarning() << "Shred: create dconfig failed: " << err;
}

bool ShredUtils::isShredEnabled()
{
    const QVariant vRe = DConfigManager::instance()->value(kShredDConfigName, "shred.enabled");
    return vRe.toBool();
}

void ShredUtils::setShredEnabled(bool enable)
{
    DConfigManager::instance()->setValue(kShredDConfigName, "shred.enabled", enable);
}

}
// namespace dfmplugin_shred
