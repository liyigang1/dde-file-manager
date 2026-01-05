// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "vaultmenumanager.h"
#include "events/vaulteventcaller.h"
#include "utils/pathmanager.h"
#include "utils/vaulthelper.h"
#include "utils/vaultautolock.h"
#include "utils/encryption/vaultconfig.h"
#include "utils/encryption/operatorcenter.h"
#include "views/datarepair/passwordverificationdialog.h"
#include "views/datarepair/datarepairstartdialog.h"
#include "views/datarepair/datarepairlockfaileddialog.h"
#include "views/datarepair/datarepairfaileddialog.h"
#include "views/datarepair/datachecknormaldialog.h"
#include "views/datarepair/datarepairsuccessdialog.h"

#include <dfm-framework/event/event.h>

using namespace dfmplugin_vault;
DWIDGET_USE_NAMESPACE
DCORE_USE_NAMESPACE

VaultMenuManager::VaultMenuManager()
{

}

VaultMenuManager *VaultMenuManager::instance()
{
    static VaultMenuManager menuHandle;
    return &menuHandle;
}

Dtk::Widget::DMenu *VaultMenuManager::createMenu()
{
    DMenu *menu = new DMenu;
    DMenu *timeMenu = new DMenu;
    switch (VaultHelper::instance()->state(PathManager::vaultLockPath())) {
    case VaultState::kNotExisted:
        menu->addAction(QObject::tr("Create Vault"), VaultHelper::instance(), &VaultHelper::createVaultDialog);
        break;
    case VaultState::kEncrypted:
        menu->addAction(QObject::tr("Unlock"), VaultHelper::instance(), &VaultHelper::unlockVaultDialog);
        menu->addSeparator();
        menu->addAction(QObject::tr("Reset Password"), VaultHelper::instance(), &VaultHelper::showResetPasswordDialog);
        menu->addAction(tr("Data detection and repair"), [] {
            VaultMenuManager::instance()->enterDataCleanup(VaultState::kEncrypted);
        });
        break;
    case VaultState::kUnlocked: {
        menu->addAction(QObject::tr("Open"), VaultHelper::instance(), &VaultHelper::openWindow);

        menu->addAction(QObject::tr("Open in new window"), VaultHelper::instance(), &VaultHelper::newOpenWindow);

        menu->addSeparator();

        VaultConfig config;
        QString encryptionMethod = config.get(kConfigNodeName, kConfigKeyEncryptionMethod, QVariant(kConfigKeyNotExist)).toString();
        if (encryptionMethod == QString(kConfigValueMethodKey) || encryptionMethod == QString(kConfigKeyNotExist)) {
            menu->addAction(QObject::tr("Lock"), []() {
                VaultHelper::instance()->lockVault(false);
            });

            QAction *timeLock = new QAction;
            timeLock->setText(QObject::tr("Auto lock"));
            VaultAutoLock::AutoLockState autoState = VaultAutoLock::instance()->getAutoLockState();
            QAction *actionNever = timeMenu->addAction(QObject::tr("Never"), []() {
                VaultAutoLock::instance()->autoLock(VaultAutoLock::AutoLockState::kNever);
            });
            actionNever->setCheckable(true);
            actionNever->setChecked(VaultAutoLock::AutoLockState::kNever == autoState ? true : false);
            timeMenu->addSeparator();
            QAction *actionFiveMins = timeMenu->addAction(QObject::tr("5 minutes"), []() {
                VaultAutoLock::instance()->autoLock(VaultAutoLock::AutoLockState::kFiveMinutes);
            });
            actionFiveMins->setCheckable(true);
            actionFiveMins->setChecked(VaultAutoLock::AutoLockState::kFiveMinutes == autoState ? true : false);
            QAction *actionTenMins = timeMenu->addAction(QObject::tr("10 minutes"), []() {
                VaultAutoLock::instance()->autoLock(VaultAutoLock::AutoLockState::kTenMinutes);
            });
            actionTenMins->setCheckable(true);
            actionTenMins->setChecked(VaultAutoLock::AutoLockState::kTenMinutes == autoState ? true : false);
            QAction *actionTwentyMins = timeMenu->addAction(QObject::tr("20 minutes"), []() {
                VaultAutoLock::instance()->autoLock(VaultAutoLock::AutoLockState::kTwentyMinutes);
            });
            actionTwentyMins->setCheckable(true);
            actionTwentyMins->setChecked(VaultAutoLock::AutoLockState::kTwentyMinutes == autoState ? true : false);
            timeLock->setMenu(timeMenu);

            menu->addMenu(timeMenu);

            menu->addSeparator();
        }

        menu->addAction(QObject::tr("Reset Password"), VaultHelper::instance(), &VaultHelper::showResetPasswordDialog);
        menu->addAction(QObject::tr("Delete File Vault"), VaultHelper::instance(), &VaultHelper::showRemoveVaultDialog);
        menu->addAction(tr("Data detection and repair"), [] {
            VaultMenuManager::instance()->enterDataCleanup(VaultState::kUnlocked);
        });
        menu->addAction(QObject::tr("Properties"), []() {
            VaultEventCaller::sendVaultProperty(VaultHelper::instance()->rootUrl());
        });
    } break;
    case VaultState::kUnderProcess:
    case VaultState::kBroken:
    case VaultState::kNotAvailable:
    case VaultState::kUnknow:
        break;
    }

    return menu;
}

void VaultMenuManager::vaultItemcontextMenuHandle(quint64 windowId, const QUrl &url, const QPoint &globalPos)
{
    if (instance()->m_isRepiaring) {
        instance()->raiseRepairingDialog();
        return;
    }

    VaultHelper::instance()->appendWinID(windowId);
    DMenu *menu = createMenu();
#ifdef ENABLE_TESTING
    dpfSlotChannel->push("dfmplugin_utils", "slot_Accessible_SetAccessibleName",
                         qobject_cast<QWidget *>(menu), AcName::kAcSidebarVaultMenu);
#endif
    QAction *act = menu->exec(globalPos);
    if (act) {
        QList<QUrl> urls { url };
        dpfSignalDispatcher->publish("dfmplugin_vault", "signal_ReportLog_MenuData", act->text(), urls);
    }
    delete menu;
}

void VaultMenuManager::enterDataCleanup(VaultState state)
{
    DataRepairStartDialog startDlg(kVaultBasePath);
    if (QDialog::Accepted != startDlg.exec())
        return;

    if (state == VaultState::kUnlocked) { // 上锁保险箱
        bool re = VaultHelper::instance()->lockVault(false);
        if (!re) {
            DataRepairLockFailedDialog lockDlg(kVaultBasePath);
            if (QDialog::Accepted != lockDlg.exec())
                return;
        }
    }

    DSecureString pwd;
    VaultConfig config;
    QString encryptionMethod = config.get(kConfigNodeName, kConfigKeyEncryptionMethod, QVariant(kConfigKeyNotExist)).toString();
    if (encryptionMethod == QString(kConfigValueMethodTransparent)) {
        pwd = OperatorCenter::getInstance()->passwordFromKeyring();
        if (pwd.isEmpty()) {
            fmCritical() << "Vault: get password from keyring failed!";
            return;
        }
    } else if (encryptionMethod == QString(kConfigValueMethodKey)) {
        PasswordVerificationDialog pwdDlg(kVaultBasePath);
        int result = pwdDlg.exec();
        if (result != QDialog::Accepted) {
            return;
        }
        pwd = pwdDlg.getCryfsPassword();
    } else {
        fmCritical() << "Vault: unknow encrypt method";
        return;
    }

    if (!m_repairDlg) {
        m_isRepiaring = true;   // 记录进入数据修复状态
        m_repairDlg = new DataRepairingDialog(kVaultBasePath, pwd);
        connect(m_repairDlg, &DataRepairingDialog::sigRepairResult,
                this, &VaultMenuManager::onRepairFinished);
        m_repairDlg->show();
    }
}

bool VaultMenuManager::isRepairing() const
{
    return m_isRepiaring;
}

void VaultMenuManager::raiseRepairingDialog()
{
    if (m_repairDlg)
        m_repairDlg->raise();
}

void VaultMenuManager::onRepairFinished(VaultCleanupResult result,
                                        const DSecureString &psw)
{
    if (m_repairDlg) {
        delete m_repairDlg;
        m_repairDlg = Q_NULLPTR;
        m_isRepiaring = false;  // 记录离开数据修复状态
    }

    if (result == kDataNormal) {
        DataCheckNormalDialog normalDlg;
        normalDlg.exec();
    } else if (result == kDataCleanupSuccess) {
        DataRepairSuccessDialog successDlg;
        successDlg.exec();
    } else {
        DataRepairFailedDialog failedDlg;
        if (QDialog::Accepted == failedDlg.exec()) {
            m_repairDlg = new DataRepairingDialog(kVaultBasePath, psw);
            connect(m_repairDlg, &DataRepairingDialog::sigRepairResult,
                    this, &VaultMenuManager::onRepairFinished);
            m_repairDlg->show();
        }
        fmCritical() << "Vault: Repairing failed, cleanup code: " << result;
    }
}
