// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "repairdialog.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QTimer>
#include <QDateTime>
#include <QFont>
#include <cmath>
#include <DLabel>
#include <DWaterProgress>

DWIDGET_USE_NAMESPACE

RepairDialog::RepairDialog(QWidget *parent)
    : DDialog(parent)
{
    setFixedSize(360, 250);
}

void RepairDialog::setDeviceInfo(const QString &deviceName, const QString &deviceSize, const QString &fsType)
{
    m_deviceName = deviceName;
    m_deviceSize = deviceSize;
    m_fsType = fsType;
}

void RepairDialog::setDevicePath(const QString &devicePath, const QString &mountPoint)
{
    m_devicePath = devicePath;
    m_mountPoint = mountPoint;
}

void RepairDialog::setErrorCode(const QString &errorCode)
{
    m_errorCode = errorCode;
}

void RepairDialog::setState(RepairState state)
{
    m_state = state;

    // 清除现有内容
    clearContents();

    switch (state) {
    case kConfirm:
        initConfirmUI();
        break;
    case kRepairing:
        initRepairingUI();
        break;
    case kSuccess:
        initSuccessUI();
        break;
    case kFailed:
        initFailedUI();
        break;
    }
}

void RepairDialog::setProgress(int percent)
{
    if (!m_progressWidget)
        return;

    if (percent >= 0 && percent <= 100) {
        m_progressWidget->setVisible(true);

        // Start DWaterProgress if not already started
        if (!m_progressWidget->property("running").toBool()) {
            m_progressWidget->start();
            m_progressWidget->setProperty("running", true);
        }
        m_progressWidget->setValue(percent);
    }
}

void RepairDialog::reject()
{
    // 修复进行中不允许关闭
    if (m_state == kRepairing) {
        return;
    }
    DDialog::reject();
}

void RepairDialog::initConfirmUI()
{
    setIcon(QIcon::fromTheme("dde-file-manager"));
    setTitle(tr("Repair Storage Device"));

    QFrame *mainFrame = new QFrame(this);
    QVBoxLayout *mainLay = new QVBoxLayout(mainFrame);

    QString subtitle = tr("Preparing to repair device: %1 (%2 %3)")
                          .arg(m_deviceName, m_deviceSize, m_fsType);
    DLabel *subtitleLabel = new DLabel(subtitle, mainFrame);
    subtitleLabel->setAlignment(Qt::AlignLeft);

    QString message = tr("The device will be unmounted during repair. "
                        "It will be automatically remounted after repair is complete. "
                        "Please do not remove the device during the repair process.");
    DLabel *msgLabel = new DLabel(message, mainFrame);
    msgLabel->setWordWrap(true);
    msgLabel->setAlignment(Qt::AlignLeft);

    mainLay->addWidget(subtitleLabel);
    mainLay->addSpacing(10);
    mainLay->addWidget(msgLabel);
    mainFrame->setLayout(mainLay);

    addContent(mainFrame);

    addButton(tr("Cancel"));
    addButton(tr("Start Repair"), true, ButtonType::ButtonRecommend);

    setCloseButtonVisible(true);
    setOnButtonClickedClose(true);
}

void RepairDialog::initRepairingUI()
{
    setIcon(QIcon::fromTheme("dde-file-manager"));
    setTitle(tr("Repairing Storage Device"));

    QFrame *mainFrame = new QFrame(this);
    QVBoxLayout *mainLay = new QVBoxLayout(mainFrame);

    QString subtitle = tr("Repairing device: %1 (%2 %3)")
                          .arg(m_deviceName, m_deviceSize, m_fsType);
    DLabel *subtitleLabel = new DLabel(subtitle, mainFrame);
    subtitleLabel->setAlignment(Qt::AlignCenter);

    // Create progress widget
    m_progressWidget = new DTK_WIDGET_NAMESPACE::DWaterProgress(mainFrame);
    m_progressWidget->setValue(0);
    m_progressWidget->setFixedSize(60, 60);

    mainLay->addWidget(subtitleLabel);
    mainLay->addSpacing(20);
    mainLay->addWidget(m_progressWidget, 0, Qt::AlignCenter);
    mainFrame->setLayout(mainLay);

    addContent(mainFrame);

    setCloseButtonVisible(false);
    setOnButtonClickedClose(false);

    // 自动启动模拟进度
    startSimulatedProgress();
}

void RepairDialog::initSuccessUI()
{
    setIcon(QIcon::fromTheme("dde-file-manager"));
    setTitle(tr("Repair Complete"));

    QFrame *mainFrame = new QFrame(this);
    QVBoxLayout *mainLay = new QVBoxLayout(mainFrame);

    QString message = tr("The device has been successfully repaired. "
                        "The device has been remounted and is now ready to use.");
    DLabel *msgLabel = new DLabel(message, mainFrame);
    msgLabel->setWordWrap(true);
    msgLabel->setAlignment(Qt::AlignCenter);

    mainLay->addSpacing(20);
    mainLay->addWidget(msgLabel);
    mainFrame->setLayout(mainLay);

    addContent(mainFrame);

    addButton(tr("Close"));
    addButton(tr("Open Device"), true, ButtonType::ButtonRecommend);

    setCloseButtonVisible(true);
    setOnButtonClickedClose(true);
}

void RepairDialog::initFailedUI()
{
    setIcon(QIcon::fromTheme("dde-file-manager"));
    setTitle(tr("Repair Failed"));

    // 失败窗口固定大小，增加宽度以容纳更多内容
    setFixedSize(400, 280);

    QFrame *mainFrame = new QFrame(this);
    QVBoxLayout *mainLay = new QVBoxLayout(mainFrame);
    mainLay->setAlignment(Qt::AlignCenter);
    mainLay->setContentsMargins(20, 10, 20, 10);

    // 计算文本宽度（预留边距）
    int maxTextWidth = 350;

    // 失败原因描述 - 分三行显示，允许换行
    QVBoxLayout *descLayout = new QVBoxLayout();
    descLayout->setAlignment(Qt::AlignCenter);
    descLayout->setSpacing(6);

    auto addDescLine = [mainFrame, maxTextWidth](QVBoxLayout *layout, const QString &text) {
        DLabel *label = new DLabel(text, mainFrame);
        label->setAlignment(Qt::AlignCenter);
        // 允许自动换行
        label->setWordWrap(true);
        // 设置固定宽度以便自动计算高度
        label->setMaximumWidth(maxTextWidth);
        label->setMinimumWidth(maxTextWidth);

        layout->addWidget(label);
    };

    addDescLine(descLayout, tr("The device may have deep format corruption or physical aging."));
    addDescLine(descLayout, tr("It is recommended to stop writing new files."));
    addDescLine(descLayout, tr("Please try using professional data recovery software or seek manual assistance."));

    mainLay->addLayout(descLayout);

    // 错误码显示
    if (!m_errorCode.isEmpty()) {
        mainLay->addSpacing(8);

        // 错误码内容 - 最多两行，超长时缩略
        DLabel *errorCodeLabel = new DLabel(mainFrame);
        errorCodeLabel->setAlignment(Qt::AlignCenter);
        // 允许自动换行
        errorCodeLabel->setWordWrap(true);
        errorCodeLabel->setMaximumWidth(maxTextWidth);

        // 设置字体样式，比描述文字小一号
        QFont errorCodeFont = errorCodeLabel->font();
        errorCodeFont.setFamily("Monospace");
        errorCodeFont.setPointSize(errorCodeFont.pointSize() - 1);
        errorCodeLabel->setFont(errorCodeFont);

        // 添加"失败原因："前缀
        QString displayText = tr("Failure reason: %1").arg(m_errorCode);

        // 计算两行最大宽度
        QFontMetrics fm(errorCodeFont);
        int maxTwoLineWidth = maxTextWidth * 2 - 100;

        // 如果超过两行宽度，截断并加省略号
        if (fm.horizontalAdvance(displayText) > maxTwoLineWidth) {
            QString elided = fm.elidedText(displayText, Qt::ElideRight, maxTwoLineWidth);
            errorCodeLabel->setText(elided);
        } else {
            errorCodeLabel->setText(displayText);
        }

        // 鼠标悬停显示完整错误码（不含前缀）
        errorCodeLabel->setToolTip(m_errorCode);

        mainLay->addWidget(errorCodeLabel);
    }

    mainLay->addStretch();
    mainFrame->setLayout(mainLay);

    addContent(mainFrame);

    addButton(tr("Close"), true);

    setCloseButtonVisible(true);
    setOnButtonClickedClose(true);
}

void RepairDialog::startSimulatedProgress()
{
    // 创建定时器用于更新进度
    if (!m_progressTimer) {
        m_progressTimer = new QTimer(this);
        connect(m_progressTimer, &QTimer::timeout, this, &RepairDialog::updateSimulatedProgress);
    }

    // 记录开始时间
    m_progressStartTime = QDateTime::currentMSecsSinceEpoch();

    // 启动进度条动画
    if (m_progressWidget && !m_progressWidget->property("running").toBool()) {
        m_progressWidget->start();
        m_progressWidget->setProperty("running", true);
    }

    // 每100毫秒更新一次进度
    m_progressTimer->start(100);
}

void RepairDialog::updateSimulatedProgress()
{
    if (m_progressWidget && m_progressWidget->value() >= 100) {
        if (m_progressTimer)
            m_progressTimer->stop();
        return;
    }

    qint64 elapsedMs = QDateTime::currentMSecsSinceEpoch() - m_progressStartTime;
    double elapsedSec = elapsedMs / 1000.0;  // 转换为秒
    int progress = 0;

    if (elapsedSec <= 120) {
        // 前2分钟：使用easeOutQuad从0%涨到90%
        double t = elapsedSec / 120.0;  // 0.0 到 1.0
        double easedT = easeOutQuad(t);  // 应用缓动函数
        progress = static_cast<int>(easedT * 90);
    } else {
        // 2分钟后：使用对数增长，越到后面越慢
        // 2分钟: 90%
        // 10分钟: 95%
        // 30分钟: 98%
        // 最终趋近99%

        double timeBeyond2Min = elapsedSec - 120.0;
        double logFactor = log(1.0 + timeBeyond2Min / 60.0) / log(29.0);  // 归一化到0-1
        double additionalProgress = logFactor * 9.0;  // 从90%再增长9%
        progress = 90 + static_cast<int>(additionalProgress);

        // 如果超过60分钟，停止在99%
        if (elapsedSec >= 3600) {
            progress = kMaxProgress;
            m_progressTimer->stop();
        }
    }

    // 确保不超过最大进度
    if (progress > kMaxProgress) {
        progress = kMaxProgress;
    }

    setProgress(progress);
}
