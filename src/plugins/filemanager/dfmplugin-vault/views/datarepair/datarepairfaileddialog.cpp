// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "datarepairfaileddialog.h"

#include <DLabel>

#include <QFrame>
#include <QVBoxLayout>

using namespace dfmplugin_vault;
DWIDGET_USE_NAMESPACE

DataRepairFailedDialog::DataRepairFailedDialog(QWidget *parent)
    : DDialog(parent)
{
    initUI();
    initConnect();
}

void DataRepairFailedDialog::onButtonClicked(int index, const QString &text)
{
    Q_UNUSED(text)
    if (index == 1) {
        accept();
    } else {
        reject();
    }
}

void DataRepairFailedDialog::initUI()
{
    setIcon(QIcon::fromTheme("dfm_vault"));
    setTitle(tr("Vault Data Repair Failed"));

    QFrame *mainFrame = new QFrame(this);
    QVBoxLayout *mainLay = new QVBoxLayout(mainFrame);

    DLabel *errorImage = new DLabel(mainFrame);
    errorImage->setPixmap(QIcon::fromTheme("dialog-error").pixmap(100, 100));

    DLabel *msg = new DLabel(tr("Vault data repair failed, please try again later~"),
                             mainFrame);
    msg->setAlignment(Qt::AlignHCenter);

    mainLay->addSpacing(20);
    mainLay->addWidget(errorImage, 0, Qt::AlignHCenter);
    mainLay->addWidget(msg);
    mainFrame->setLayout(mainLay);

    addContent(mainFrame);

    addButton(tr("Cancel"));
    addButton(tr("Repair Again"));

    setOnButtonClickedClose(false);
}

void DataRepairFailedDialog::initConnect()
{
    connect(this, &DataRepairFailedDialog::buttonClicked,
            this, &DataRepairFailedDialog::onButtonClicked);
}
