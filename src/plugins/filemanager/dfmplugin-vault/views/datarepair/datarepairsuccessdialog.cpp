// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "datarepairsuccessdialog.h"

#include <DLabel>

#include <QFrame>
#include <QVBoxLayout>

using namespace dfmplugin_vault;
DWIDGET_USE_NAMESPACE

DataRepairSuccessDialog::DataRepairSuccessDialog(QWidget *parent)
    : DDialog(parent)
{
    initUI();
}

void DataRepairSuccessDialog::initUI()
{
    setIcon(QIcon::fromTheme("dfm_vault"));
    setTitle(tr("Vault Data Repair Completed"));

    QFrame *mainFrame = new QFrame(this);
    QVBoxLayout *mainLay = new QVBoxLayout(mainFrame);

    DLabel *okImage = new DLabel(mainFrame);
    okImage->setPixmap(QIcon::fromTheme("dialog-ok").pixmap(100, 100));
    okImage->setAlignment(Qt::AlignHCenter);

    DLabel *msg = new DLabel(tr("Vault data repair completed,\n "
                                "you can now use it with confidence!"),
                             mainFrame);
    msg->setAlignment(Qt::AlignHCenter);

    mainLay->addSpacing(20);
    mainLay->addWidget(okImage);
    mainLay->addWidget(msg);
    mainFrame->setLayout(mainLay);

    addContent(mainFrame);

    addButton(tr("OK"));
}
