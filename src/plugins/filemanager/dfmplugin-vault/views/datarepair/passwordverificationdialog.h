// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PASSWORDVERIFICATIONDIALOG_H
#define PASSWORDVERIFICATIONDIALOG_H

#include "dfmplugin_vault_global.h"

#include <QPushButton>
#include <QFutureWatcher>

#include <DDialog>
#include <DPasswordEdit>
#include <DSecureString>
#include <DSpinner>

namespace dfmplugin_vault {

class PasswordVerificationDialog : public DTK_WIDGET_NAMESPACE::DDialog
{
    Q_OBJECT

    struct VerificationResult {
        int result;
        DTK_CORE_NAMESPACE::DSecureString cryfsPsw;
    };

public:
    explicit PasswordVerificationDialog(const QString &baseDirPath,
                                        QWidget *parent = Q_NULLPTR);
    DTK_CORE_NAMESPACE::DSecureString getCryfsPassword();

public Q_SLOTS:
    void onBtnClicked(int index, const QString &text);
    void showPasswordHint();
    void updatePwdEditAlertState();
    void verificationFinished();

private:
    void initUI();
    void initConnect();

    QString m_baseDirPath;
    DTK_CORE_NAMESPACE::DSecureString m_cryfsPsw;
    DTK_WIDGET_NAMESPACE::DPasswordEdit *m_pwdEdit { Q_NULLPTR };
    QPushButton *m_tipsButton { Q_NULLPTR };
    QFutureWatcher<VerificationResult> *m_veriWatcher { Q_NULLPTR };
    DTK_WIDGET_NAMESPACE::DSpinner *m_spinner { Q_NULLPTR };
};

}

#endif
