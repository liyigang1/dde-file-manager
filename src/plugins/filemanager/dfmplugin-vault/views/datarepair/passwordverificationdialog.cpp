// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "passwordverificationdialog.h"
#include "utils/pathmanager.h"
#include "utils/encryption/passwordmanager.h"
#include "utils/encryption/vaultconfig.h"
#include "utils/encryption/operatorcenter.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QtConcurrent>

using namespace dfmplugin_vault;
DWIDGET_USE_NAMESPACE
DCORE_USE_NAMESPACE

PasswordVerificationDialog::PasswordVerificationDialog(const QString &baseDirPath,
                                                       QWidget *parent)
    : DDialog(parent)
    , m_baseDirPath(baseDirPath)
{
    initUI();
    initConnect();
}

DSecureString PasswordVerificationDialog::getCryfsPassword()
{
    return m_cryfsPsw;
}

void PasswordVerificationDialog::onBtnClicked(int index, const QString &text)
{
    Q_UNUSED(text)

    if (1 == index) {   // 密码认证
        QString password = m_pwdEdit->text();
        QString baseDirPath = m_baseDirPath;

        m_spinner->move((width() - m_spinner->width())/2, (height() - m_spinner->height())/2);
        m_spinner->show();
        m_spinner->raise();
        m_spinner->start();

        QFuture<VerificationResult> future = QtConcurrent::run([baseDirPath, password]()->VerificationResult {
            VerificationResult re;

            if (!OperatorCenter::getInstance()->isVersionUsedLuksContainer()) {
                // 升级前老版本：按解密逻辑通过 checkPassword 获取 cipher
                QString cipher;
                if (!OperatorCenter::getInstance()->checkPassword(password, cipher)) {
                    re.result = -1;
                    re.cryfsPsw = "";
                    return re;
                }
                re.result = 0;
                re.cryfsPsw = cipher.toUtf8();
                return re;
            }

            // 新版本：从 LUKS 容器导出主密钥
            QString containerPath = PathManager::vaultPswContainerPath(baseDirPath);
            char masterKeyBuf[64] = { 0 };
            size_t masterKeySize = 64;
            int ret = PasswordManager::exportMasterKey(containerPath.toUtf8().constData(),
                                                       password.toUtf8().constData(),
                                                       masterKeyBuf, &masterKeySize);
            re.result = ret;

            if (ret < 0) {
                re.cryfsPsw = "";
                return re;
            }

            QByteArray cryfsPassword = QByteArray(masterKeyBuf, static_cast<int>(masterKeySize));
            VaultConfig config;
            QString creationType = config.getVaultCreationType();
            if (creationType == kConfigValueVaultCreationTypeMigrated) {
                while (cryfsPassword.endsWith('\0'))
                    cryfsPassword.chop(1);
            }
            re.cryfsPsw = cryfsPassword;
            return re;
        });
        m_veriWatcher->setFuture(future);
    } else {
        reject();
    }
}

void PasswordVerificationDialog::showPasswordHint()
{
    QString pwdHint("");
    if (OperatorCenter::getInstance()->getPasswordHint(pwdHint)) {
        QString hint = tr("Password hint: %1").arg(pwdHint);
        m_pwdEdit->showAlertMessage(hint);
    }
}

void PasswordVerificationDialog::updatePwdEditAlertState()
{
    m_pwdEdit->setAlert(false);
}

void PasswordVerificationDialog::verificationFinished()
{
    m_spinner->stop();
    m_spinner->hide();

    VerificationResult re = m_veriWatcher->result();
    if (re.result < 0) {
        m_pwdEdit->setAlert(true);
        m_pwdEdit->showAlertMessage(tr("Password verification failed"));
        return;
    }

    m_cryfsPsw = re.cryfsPsw;
    accept();
}

void PasswordVerificationDialog::initUI()
{
    setIcon(QIcon::fromTheme("dfm_vault"));
    setTitle(tr("Please Input Vault Password"));

    QFrame *mainFrame = new QFrame(this);
    QVBoxLayout *mainLay = new QVBoxLayout(mainFrame);

    QHBoxLayout *itemLay = new QHBoxLayout();
    m_pwdEdit = new DPasswordEdit(mainFrame);
    m_pwdEdit->lineEdit()->setPlaceholderText(tr("Password"));
    m_pwdEdit->setAttribute(Qt::WA_InputMethodEnabled, false);

    m_tipsButton =  new QPushButton(mainFrame);
    m_tipsButton->setIcon(QIcon::fromTheme("dfm_vault_tips"));

    itemLay->addWidget(m_pwdEdit);
    itemLay->addWidget(m_tipsButton);

    mainLay->addSpacing(20);
    mainLay->addLayout(itemLay);
    mainLay->addSpacing(10);

    mainFrame->setLayout(mainLay);
    mainFrame->setFixedWidth(350);
    addContent(mainFrame, Qt::AlignHCenter);

    addButton(tr("Cancel"));
    addButton(tr("Verify Key"), true, ButtonType::ButtonRecommend);

    setOnButtonClickedClose(false);

    m_spinner = new DSpinner(this);
    m_spinner->setFixedSize(48, 48);
    m_spinner->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_spinner->setFocusPolicy(Qt::NoFocus);
    m_spinner->hide();

    m_veriWatcher = new QFutureWatcher<VerificationResult>(this);
}

void PasswordVerificationDialog::initConnect()
{
    connect(this, &PasswordVerificationDialog::buttonClicked,
            this, &PasswordVerificationDialog::onBtnClicked);
    connect(m_pwdEdit, &DPasswordEdit::textChanged,
            this, &PasswordVerificationDialog::updatePwdEditAlertState);
    connect(m_tipsButton, &QPushButton::clicked,
            this, &PasswordVerificationDialog::showPasswordHint);
    connect(m_veriWatcher, &QFutureWatcher<VerificationResult>::finished,
            this, &PasswordVerificationDialog::verificationFinished);
}
