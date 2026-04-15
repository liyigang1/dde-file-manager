// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef REPAIRDIALOG_H
#define REPAIRDIALOG_H

#include <DDialog>
#include <DWaterProgress>
#include <DSpinner>

#include <QMap>

class RepairDialog : public DTK_WIDGET_NAMESPACE::DDialog
{
    Q_OBJECT

public:
    enum RepairState {
        kConfirm,       // 确认是否修复
        kRepairing,     // 修复进行中
        kSuccess,       // 修复成功
        kFailed         // 修复失败
    };

    explicit RepairDialog(QWidget *parent = nullptr);

    void setDeviceInfo(const QString &deviceName, const QString &deviceSize, const QString &fsType);
    void setDevicePath(const QString &devicePath, const QString &mountPoint);
    void setState(RepairState state);
    void setProgress(int percent);

    QString devicePath() const { return m_devicePath; }
    QString mountPoint() const { return m_mountPoint; }

    void reject() override;

private:
    void initConfirmUI();
    void initRepairingUI();
    void initSuccessUI();
    void initFailedUI();

    QString m_deviceName;
    QString m_deviceSize;
    QString m_fsType;
    QString m_devicePath;        // Device DBus path
    QString m_mountPoint;        // Device mount point
    RepairState m_state { kConfirm };

    DTK_WIDGET_NAMESPACE::DWaterProgress *m_progressWidget { nullptr };
    DTK_WIDGET_NAMESPACE::DSpinner *m_spinnerWidget { nullptr };
};

#endif   // REPAIRDIALOG_H
