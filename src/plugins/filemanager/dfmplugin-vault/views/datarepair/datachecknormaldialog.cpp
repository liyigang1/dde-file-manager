// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "datachecknormaldialog.h"

#include <DLabel>

#include <QFrame>
#include <QVBoxLayout>

using namespace dfmplugin_vault;
DWIDGET_USE_NAMESPACE
DCORE_USE_NAMESPACE

DataCheckNormalDialog::DataCheckNormalDialog(QWidget *parent)
    : DDialog(parent)
{
    initUI();
}

void DataCheckNormalDialog::initUI()
{
    setIcon(QIcon::fromTheme("dfm_vault"));
    setTitle(tr("Vault Data Check is Normal"));

    QFrame *mainFrame = new QFrame(this);
    QVBoxLayout *mainLay = new QVBoxLayout(mainFrame);

    DLabel *finishedImage = new DLabel(mainFrame);
    finishedImage->setPixmap(QIcon::fromTheme("dialog-ok").pixmap(100, 100));

    DLabel *msg = new DLabel(tr("Vault integrity verified, all files normal\n, "
                                "safe to use!"), mainFrame);
    msg->setAlignment(Qt::AlignHCenter);

    mainLay->addSpacing(20);
    mainLay->addWidget(finishedImage, 0, Qt::AlignHCenter);
    mainLay->addWidget(msg);
    mainFrame->setLayout(mainLay);

    addContent(mainFrame);

    addButton(tr("OK"));
}
