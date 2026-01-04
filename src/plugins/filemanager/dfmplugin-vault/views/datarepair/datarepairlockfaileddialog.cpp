// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "datarepairlockfaileddialog.h"
#include "utils/vaulthelper.h"

#include <DLabel>

#include <QFrame>
#include <QVBoxLayout>

using namespace dfmplugin_vault;
DWIDGET_USE_NAMESPACE

DataRepairLockFailedDialog::DataRepairLockFailedDialog(const QString &baseDir,
                                                       QWidget *parent)
    : DDialog(parent)
    , m_baseDirPath(baseDir)
{
    initUI();
    initConnect();
}

void DataRepairLockFailedDialog::onButtonClicked(int index, const QString &text)
{
    Q_UNUSED(text)

    if (index == 1) {
        bool re = VaultHelper::instance()->lockVault(false);
        if (re)
            accept();
    } else {
        reject();
    }
}

void DataRepairLockFailedDialog::initUI()
{
    setIcon(QIcon::fromTheme("dfm_vault"));
    setTitle(tr("Vault Data Check and Repair"));

    QFrame *mainFrame =  new QFrame(this);
    QVBoxLayout *mainLay = new QVBoxLayout(mainFrame);

    DLabel *warnningImage = new DLabel(mainFrame);
    warnningImage->setPixmap(QIcon::fromTheme("dfm_vault_active_start").pixmap(88, 100));
    warnningImage->setAlignment(Qt::AlignHCenter);

    DLabel *msg = new DLabel(tr("<font color='red'>*The vault will be locked before repair,<br> "
                                "Please save and close any files that are currently being edited,<br> "
                                "then click \"Check and Repair Now\".</font>"),
                             mainFrame);
    msg->setTextFormat(Qt::RichText);
    msg->setAlignment(Qt::AlignHCenter);

    mainLay->addSpacing(20);
    mainLay->addWidget(warnningImage);
    mainLay->addWidget(msg);
    mainFrame->setLayout(mainLay);
    addContent(mainFrame);

    addButton(tr("Cancel"));
    addButton(tr("Check and Repair Now"), true, ButtonType::ButtonRecommend);

    setOnButtonClickedClose(false);
}

void DataRepairLockFailedDialog::initConnect()
{
    connect(this, &DataRepairLockFailedDialog::buttonClicked,
            this, &DataRepairLockFailedDialog::onButtonClicked);
}
