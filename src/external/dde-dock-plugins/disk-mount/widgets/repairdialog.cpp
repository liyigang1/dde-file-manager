// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "repairdialog.h"

#include <QFrame>
#include <QVBoxLayout>
#include <DLabel>
#include <DSpinner>
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
    if (!m_progressWidget || !m_spinnerWidget)
        return;

    if (percent >= 0 && percent <= 100) {
        // Valid progress: show DWaterProgress, hide DSpinner
        m_spinnerWidget->setVisible(false);
        m_progressWidget->setVisible(true);

        // Start DWaterProgress if not already started
        if (!m_progressWidget->property("running").toBool()) {
            m_progressWidget->start();
            m_progressWidget->setProperty("running", true);
        }
        m_progressWidget->setValue(percent);
    } else {
        // Invalid progress: show DSpinner, hide DWaterProgress
        m_progressWidget->setVisible(false);
        m_spinnerWidget->setVisible(true);
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

    // Create both progress widgets
    m_progressWidget = new DTK_WIDGET_NAMESPACE::DWaterProgress(mainFrame);
    m_progressWidget->setValue(0);
    m_progressWidget->setFixedSize(60, 60);

    m_spinnerWidget = new DTK_WIDGET_NAMESPACE::DSpinner(mainFrame);
    m_spinnerWidget->setFixedSize(60, 60);
    m_spinnerWidget->start();

    // Initially show spinner, hide progress
    m_progressWidget->setVisible(false);
    m_spinnerWidget->setVisible(true);

    mainLay->addWidget(subtitleLabel);
    mainLay->addSpacing(20);
    mainLay->addWidget(m_progressWidget, 0, Qt::AlignCenter);
    mainLay->addWidget(m_spinnerWidget, 0, Qt::AlignCenter);
    mainFrame->setLayout(mainLay);

    addContent(mainFrame);

    setCloseButtonVisible(false);
    setOnButtonClickedClose(false);
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

    QFrame *mainFrame = new QFrame(this);
    QVBoxLayout *mainLay = new QVBoxLayout(mainFrame);

    QString message = tr("Failed to repair the device. This may be due to serious format errors "
                        "or physical damage. To protect your data, it is recommended to stop "
                        "writing new files and try using professional data recovery software "
                        "or seek manual assistance.");
    DLabel *msgLabel = new DLabel(message, mainFrame);
    msgLabel->setWordWrap(true);
    msgLabel->setAlignment(Qt::AlignCenter);

    mainLay->addSpacing(20);
    mainLay->addWidget(msgLabel);
    mainFrame->setLayout(mainLay);

    addContent(mainFrame);

    addButton(tr("Close"), true);

    setCloseButtonVisible(true);
    setOnButtonClickedClose(true);
}
