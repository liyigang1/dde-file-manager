// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VAULTMENUMANAGER_H
#define VAULTMENUMANAGER_H

#include "dfmplugin_vault_global.h"
#include "views/datarepair/datarepairingdialog.h"

#include <DMenu>
#include <DSecureString>

namespace dfmplugin_vault {

class VaultMenuManager final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(VaultMenuManager)

private:
    explicit VaultMenuManager();

public:
    static VaultMenuManager *instance();
    static DTK_WIDGET_NAMESPACE::DMenu *createMenu();
    static void vaultItemcontextMenuHandle(quint64 windowId, const QUrl &url, const QPoint &globalPos);
    void enterDataCleanup(VaultState state);
    bool isRepairing() const;
    void raiseRepairingDialog();

private Q_SLOTS:
    void onRepairFinished(VaultCleanupResult result,
                          const DTK_CORE_NAMESPACE::DSecureString &psw);

private:
    DataRepairingDialog *m_repairDlg { Q_NULLPTR };
    bool m_isRepiaring { false };
};

}

#endif
