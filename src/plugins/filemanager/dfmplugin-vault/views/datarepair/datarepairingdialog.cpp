// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "datarepairingdialog.h"
#include "utils/pathmanager.h"

#include <DLabel>

#include <QFrame>
#include <QVBoxLayout>
#include <QProcess>

inline constexpr int kTreshold { 99 };
inline constexpr int kExitCodeOfDataOk { 0 };
inline constexpr int kExitCodeOfCleanupSuccess { 21 };

using namespace dfmplugin_vault;
DWIDGET_USE_NAMESPACE
DCORE_USE_NAMESPACE

DataRepairingDialog::DataRepairingDialog(const QString &baseDir,
                                         const DSecureString &psw,
                                         QWidget *parent)
    : DDialog(parent)
    , m_baseDir(baseDir)
    , m_psw(psw)
{
    initUI();
    initConnect();
}

DataRepairingDialog::~DataRepairingDialog()
{
    if (m_progressTimer.isActive())
        m_progressTimer.stop();
    if (m_waterProgress)
        m_waterProgress->stop();

    // 终止正在运行的进程
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->disconnect();  // 断开所有信号连接
        m_process->terminate();
        if (!m_process->waitForFinished(3000)) {
            m_process->kill();
        }
    }
}

void DataRepairingDialog::reject()
{
    // 什么都不做，达到屏蔽 Esc 键和默认的关闭行为
}

void DataRepairingDialog::onProgressTimerTimeout()
{
    if (m_waterProgress) {
        int curProgress = m_progressValue++;
        if (curProgress > kTreshold)
            curProgress = kTreshold;
        m_waterProgress->setValue(curProgress);
        if (curProgress == 1) {
            asynRepairingVault(m_baseDir, m_psw);
        }
    }
}

void DataRepairingDialog::initUI()
{
    setIcon(QIcon::fromTheme("dfm_vault"));
    setTitle(tr("Vault Data Check and Repair"));

    QFrame *mainFrame = new QFrame(this);
    QVBoxLayout *mainLay = new QVBoxLayout(mainFrame);

    m_waterProgress = new DWaterProgress(mainFrame);
    m_waterProgress->setValue(0);
    m_waterProgress->start();

    DLabel *msg = new DLabel(tr("Data health check in progress, please wait..."), mainFrame);
    msg->setAlignment(Qt::AlignHCenter);

    mainLay->addSpacing(20);
    mainLay->addWidget(m_waterProgress, 0, Qt::AlignHCenter);
    mainLay->addWidget(msg);
    mainFrame->setLayout(mainLay);

    addContent(mainFrame);

    setOnButtonClickedClose(false);
    setCloseButtonVisible(false);

    m_process = new QProcess(this);
    m_progressValue = 0;
    m_progressTimer.setSingleShot(false);
    m_progressTimer.start(1000);
}

void DataRepairingDialog::initConnect()
{
    connect(&m_progressTimer, &QTimer::timeout, this, &DataRepairingDialog::onProgressTimerTimeout);
    connect(m_process, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, &DataRepairingDialog::onProcessFinished);
}

void DataRepairingDialog::asynRepairingVault(const QString &baseDir,
                                             const DSecureString &psw)
{
    m_process->setProgram("cryfs");
    m_process->setArguments({ "--repair-corrupted-blocks",
                           PathManager::vaultEncryptPath(baseDir),
                           PathManager::vaultMountPath(baseDir) });
    m_process->start();
    if (m_process->waitForStarted()) {
        m_process->write(QString(psw).toUtf8());
        m_process->write("\n");
        m_process->closeWriteChannel();
    } else {
        fmCritical() << "Failed to start cryfs process";
    }
}

void DataRepairingDialog::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_progressTimer.stop();
    m_waterProgress->stop();

    VaultCleanupResult result { VaultCleanupResult::kUnknowCleanupResult };
    if (exitStatus == QProcess::NormalExit) {
        if (exitCode == kExitCodeOfDataOk) {    // 数据正常
            result = VaultCleanupResult::kDataNormal;
        } else if (exitCode == kExitCodeOfCleanupSuccess) {    // 数据冗余，且已成功清理
            result = VaultCleanupResult::kDataCleanupSuccess;
        } else {    // 数据冗余，清理失败
            result = VaultCleanupResult::kDataCleanupFailed;
            fmCritical() << "Vault: Cleanup data failed, exit code: " << exitCode;
        }
    } else {
        result = VaultCleanupResult::kProgressCrash;
        fmCritical() << "Vault: cleanup data process crashed";
    }
    accept();
    emit sigRepairResult(result, m_psw);
}
