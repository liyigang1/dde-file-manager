// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "datarepairstartdialog.h"

#include <QFrame>
#include <QVBoxLayout>
#include <DLabel>

using namespace dfmplugin_vault;
DWIDGET_USE_NAMESPACE

DataRepairStartDialog::DataRepairStartDialog(const QString &baseDir,
                                             QWidget *parent)
    : DDialog(parent)
    , m_baseDirPath(baseDir)
{
    initUI();
    initConnect();
}

void DataRepairStartDialog::onButtonClicked(int index, const QString &text)
{
    Q_UNUSED(text)
    if (index == 0) {
        reject();
    } else if (index == 1) {
        accept();
    }
}

void DataRepairStartDialog::initUI()
{
    setIcon(QIcon::fromTheme("dfm_vault"));
    setTitle(tr("Vault Data Check and Repair"));

    QFrame *mainFrame = new QFrame(this);
    QVBoxLayout *mainLay = new QVBoxLayout(mainFrame);

    DLabel *fixImage = new DLabel(mainFrame);
    fixImage->setPixmap(QIcon::fromTheme("dfm_vault_active_start").pixmap(88, 100));
    fixImage->setAlignment(Qt::AlignHCenter);

    DLabel *msg = new DLabel(tr("The vault may experience abnormal situations such as power failure,<br>"
                                "which could result in certain files becoming unreadable.<br>"
                                "Clicked \"Check and Repair Now\" to quickly check the file status<br> "
                                "and troubleshoot/repair any corruputed data.<br>"
                                "<font color='red'>*Please save and close any files that are currently being<br> "
                                "edited before starting the repair process</font>"),
                             mainFrame);
    msg->setTextFormat(Qt::RichText);
    msg->setAlignment(Qt::AlignHCenter);

    mainLay->addSpacing(20);
    mainLay->addWidget(fixImage);
    mainLay->addWidget(msg);
    mainFrame->setLayout(mainLay);
    addContent(mainFrame);

    addButton(tr("Cancel"));
    addButton(tr("Check and Repair Now"), true, ButtonType::ButtonRecommend);

    setOnButtonClickedClose(false);
}

void DataRepairStartDialog::initConnect()
{
    connect(this, &DataRepairStartDialog::buttonClicked,
            this, &DataRepairStartDialog::onButtonClicked);
}
