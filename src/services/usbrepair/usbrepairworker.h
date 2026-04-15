// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef USBREPAIRWORKER_H
#define USBREPAIRWORKER_H

#include "service_usbrepair_global.h"

#include <QObject>
#include <QProcess>
#include <QSet>

SERVICEUSBREPAIR_BEGIN_NAMESPACE

class UsbRepairWorker : public QObject
{
    Q_OBJECT

public:
    explicit UsbRepairWorker(QObject *parent = nullptr);
    ~UsbRepairWorker();

    bool startRepair(const QString &devicePath, const QString &callerBusName, QString &errorMessage);
    bool cancelRepair(const QString &devicePath);
    bool isRepairing(const QString &devicePath) const;

Q_SIGNALS:
    void progress(const QString &devicePath, int percent, const QString &logLine);
    void finished(const QString &devicePath, bool success, const QString &summary);

private Q_SLOTS:
    void onFsckReadyRead();
    void onFsckFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onFsckTimeout();

private:
    bool checkAuthorization(const QString &callerBusName);
    bool umountDevice(const QString &devicePath);
    QString detectFsType(const QString &devicePath);
    bool validateDevicePath(const QString &devicePath);
    void executeFsck(const QString &devicePath, const QString &fsType);
    QString blockObjPathFromDevice(const QString &devicePath);

    QProcess *m_currentProcess { nullptr };
    QString m_currentDevice;
    QString m_currentFsType;
    QString m_fsckOutput;
    QSet<QString> m_activeRepairs;
};

SERVICEUSBREPAIR_END_NAMESPACE

#endif   // USBREPAIRWORKER_H
