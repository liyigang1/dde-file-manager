// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DATAREPAIRINGDIALOG_H
#define DATAREPAIRINGDIALOG_H

#include "dfmplugin_vault_global.h"

#include <DDialog>
#include <DWaterProgress>
#include <DSecureString>

#include <QTimer>
#include <QProcess>

namespace dfmplugin_vault {

class DataRepairingDialog : public DTK_WIDGET_NAMESPACE::DDialog
{
    Q_OBJECT

public:
    explicit DataRepairingDialog(const QString &baseDir,
                                 const DTK_CORE_NAMESPACE::DSecureString &psw,
                                 QWidget *parent = Q_NULLPTR);
    ~DataRepairingDialog() override;

    void reject() override;

Q_SIGNALS:
    void sigRepairResult(VaultCleanupResult result,
                         const DTK_CORE_NAMESPACE::DSecureString psw);

private Q_SLOTS:
    void onProgressTimerTimeout();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void initUI();
    void initConnect();
    void asynRepairingVault(const QString &baseDir,
                            const DTK_CORE_NAMESPACE::DSecureString &psw);

    QString m_baseDir;
    DTK_CORE_NAMESPACE::DSecureString m_psw;
    DTK_WIDGET_NAMESPACE::DWaterProgress *m_waterProgress { Q_NULLPTR };
    QTimer m_progressTimer;
    int m_progressValue { 0 };
    QProcess *m_process { Q_NULLPTR };
};


}

#endif
