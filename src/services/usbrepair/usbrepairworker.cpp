// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "usbrepairworker.h"

#include <QDBusInterface>
#include <QFile>
#include <QTimer>
#include <QProcess>
#include <QRegularExpression>

#include <polkit-qt5-1/PolkitQt1/Authority>
#include <errno.h>

SERVICEUSBREPAIR_USE_NAMESPACE

UsbRepairWorker::UsbRepairWorker(QObject *parent)
    : QObject(parent)
{
}

UsbRepairWorker::~UsbRepairWorker()
{
    if (m_currentProcess) {
        m_currentProcess->kill();
        m_currentProcess->waitForFinished(5000);
    }
}

bool UsbRepairWorker::startRepair(const QString &devicePath, const QString &callerBusName,
                                  QString &errorMessage)
{
    // 1. Validate device path
    if (!validateDevicePath(devicePath)) {
        errorMessage = tr("Invalid device path: %1").arg(devicePath);
        return false;
    }

    // 2. Idempotency check
    if (isRepairing(devicePath)) {
        errorMessage = tr("Device is already being repaired: %1").arg(devicePath);
        return false;
    }

    // 3. Detect filesystem type
    QString fsType = detectFsType(devicePath);
    if (fsType.isEmpty()) {
        errorMessage = tr("Cannot detect filesystem type for: %1").arg(devicePath);
        return false;
    }

    // 4. Whitelist check
    if (!Defines::kSupportedFsTypes.contains(fsType)) {
        errorMessage = tr("Unsupported filesystem type: %1").arg(fsType);
        return false;
    }

    // 5. Check hardware read-only
    QProcess blockdevProc;
    QString wholeDisk = devicePath;
    QRegularExpression re(R"((.+)[0-9]+$)");
    QRegularExpressionMatch match = re.match(devicePath);
    if (match.hasMatch())
        wholeDisk = match.captured(1);

    blockdevProc.start("blockdev", { "--getro", wholeDisk });
    blockdevProc.waitForFinished(3000);
    if (blockdevProc.exitCode() == 0 && blockdevProc.readAllStandardOutput().trimmed() == "1") {
        errorMessage = tr("Device is hardware write-protected, cannot repair");
        return false;
    }

    // 6. PolKit authorization
    if (!checkAuthorization(callerBusName)) {
        errorMessage = tr("Authorization failed");
        return false;
    }

    // 7. Umount if mounted
    bool wasMounted = false;
    QFile mounts("/proc/mounts");
    if (mounts.open(QIODevice::ReadOnly)) {
        QByteArray data = mounts.readAll();
        mounts.close();
        for (const QByteArray &line : data.split('\n')) {
            if (line.startsWith(devicePath.toUtf8() + " ")) {
                wasMounted = true;
                break;
            }
        }
    }

    if (wasMounted && !umountDevice(devicePath)) {
        errorMessage = tr("Failed to unmount device: %1").arg(devicePath);
        return false;
    }

    // 8. Start repair
    m_currentDevice = devicePath;
    m_currentFsType = fsType;
    m_activeRepairs.insert(devicePath);
    executeFsck(devicePath, fsType);

    return true;
}

bool UsbRepairWorker::cancelRepair(const QString &devicePath)
{
    if (m_currentDevice != devicePath || !m_currentProcess)
        return false;

    m_currentProcess->kill();
    return true;
}

bool UsbRepairWorker::isRepairing(const QString &devicePath) const
{
    return m_activeRepairs.contains(devicePath);
}

void UsbRepairWorker::onFsckReadyRead()
{
    if (!m_currentProcess)
        return;

    // Read both stdout and stderr and accumulate
    QString stdout = QString::fromUtf8(m_currentProcess->readAllStandardOutput());
    QString stderr = QString::fromUtf8(m_currentProcess->readAllStandardError());
    QString output = stdout + stderr;

    // Accumulate for final logging
    m_fsckOutput += output;

    for (const QString &line : output.split('\n', QString::SkipEmptyParts)) {
        // Parse progress percentage from output
        int percent = -1;   // -1 = indeterminate
        QRegularExpression percentRe(R"((\d+)%)");
        QRegularExpressionMatch match = percentRe.match(line);
        if (match.hasMatch())
            percent = match.captured(1).toInt();

        emit progress(m_currentDevice, percent, line);
    }
}

void UsbRepairWorker::onFsckFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus)

    QString device = m_currentDevice;
    QString fsType = m_currentFsType;
    QString summary;
    bool success = false;

    // Read any remaining output that wasn't captured by onFsckReadyRead
    QString remainingOutput = QString::fromUtf8(m_currentProcess->readAllStandardError())
            + QString::fromUtf8(m_currentProcess->readAllStandardOutput());
    m_fsckOutput += remainingOutput;

    fmInfo() << "UsbRepairWorker: fsck finished | device:" << device << "| fstype:" << fsType << "| exitCode:" << exitCode << "| output:" << m_fsckOutput;

    if (exitCode == 0) {
        success = true;
        summary = tr("Filesystem repair completed successfully");
    } else if (fsType == "ntfs" && exitCode != 0) {
        success = false;
        summary = tr("NTFS repair capability is limited on Linux. "
                     "Please connect the device to a Windows system for deep repair.");
    } else {
        success = (exitCode == 1);   // exit code 1 = errors corrected
        if (success) {
            summary = tr("Filesystem errors have been repaired");
        } else {
            summary = tr("Filesystem repair failed (exit code: %1)").arg(exitCode);
            if (!m_fsckOutput.trimmed().isEmpty())
                summary += "\n" + m_fsckOutput.trimmed();
        }
    }

    m_activeRepairs.remove(device);
    m_currentDevice.clear();
    m_currentFsType.clear();
    m_currentProcess->deleteLater();
    m_currentProcess = nullptr;

    emit finished(device, success, summary);
}

void UsbRepairWorker::onFsckTimeout()
{
    if (m_currentProcess) {
        m_currentProcess->kill();
        emit finished(m_currentDevice, false, tr("Repair timed out after %1 seconds")
                      .arg(Defines::kFsckTimeoutMs / 1000));
    }
}

bool UsbRepairWorker::checkAuthorization(const QString &callerBusName)
{
    using namespace PolkitQt1;

    if (callerBusName.isEmpty())
        return false;

    Authority::Result result = Authority::instance()->checkAuthorizationSync(
        Defines::kPolkitActionId,
        SystemBusNameSubject(callerBusName),
        Authority::AllowUserInteraction);

    return result == Authority::Yes;
}

bool UsbRepairWorker::umountDevice(const QString &devicePath)
{
    QProcess proc;
    proc.start("umount", { devicePath });
    proc.waitForFinished(10000);
    if (proc.exitCode() != 0) {
        fmWarning() << "UsbRepairWorker: umount failed:" << proc.readAllStandardError();
        return false;
    }
    return true;
}

QString UsbRepairWorker::detectFsType(const QString &devicePath)
{
    QProcess proc;

    // Method 1: blkid (fastest, works for normal cases)
    proc.start("blkid", { "-o", "value", "-s", "TYPE", devicePath });
    proc.waitForFinished(5000);
    if (proc.exitCode() == 0) {
        QString output = proc.readAllStandardOutput().trimmed();
        if (!output.isEmpty())
            return output;
    }

    // Method 2: file command (fallback for corrupted filesystems)
    // Reads magic numbers and raw structures even when filesystem is severely damaged
    proc.start("file", { "-s", devicePath });
    proc.waitForFinished(5000);
    QString fileOutput = proc.readAllStandardOutput();
    fmInfo() << "UsbRepairWorker: command: file -s" << devicePath << "| exitCode:" << proc.exitCode() << "| output:" << fileOutput;
    if (proc.exitCode() == 0) {
        // Parse file command output for filesystem signatures
        if (fileOutput.contains("FAT", Qt::CaseInsensitive))
            return "vfat";
        else if (fileOutput.contains("exFAT", Qt::CaseInsensitive))
            return "exfat";
        else if (fileOutput.contains("NTFS", Qt::CaseInsensitive))
            return "ntfs";
        else if (fileOutput.contains("ext2 filesystem", Qt::CaseInsensitive) ||
                 fileOutput.contains("ext3 filesystem", Qt::CaseInsensitive) ||
                 fileOutput.contains("ext4 filesystem", Qt::CaseInsensitive))
            return "ext4";
    }

    return {};
}

bool UsbRepairWorker::validateDevicePath(const QString &devicePath)
{
    if (devicePath.isEmpty())
        return false;
    if (!devicePath.startsWith("/dev/"))
        return false;
    // Must not contain path traversal
    if (devicePath.contains(".."))
        return false;

    return QFile::exists(devicePath);
}

QString UsbRepairWorker::blockObjPathFromDevice(const QString &devicePath)
{
    // Convert /dev/sdb1 to /org/freedesktop/UDisks2/block_devices/sdb1
    QString basename = devicePath.mid(5);  // Remove "/dev/"
    return QString("/org/freedesktop/UDisks2/block_devices/%1").arg(basename);
}

void UsbRepairWorker::executeFsck(const QString &devicePath, const QString &fsType)
{
    m_currentProcess = new QProcess(this);
    connect(m_currentProcess, &QProcess::readyReadStandardOutput,
            this, &UsbRepairWorker::onFsckReadyRead);
    connect(m_currentProcess, &QProcess::readyReadStandardError,
            this, &UsbRepairWorker::onFsckReadyRead);
    connect(m_currentProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &UsbRepairWorker::onFsckFinished);

    // Reset output accumulator
    m_fsckOutput.clear();

    // Timeout protection
    QTimer::singleShot(Defines::kFsckTimeoutMs, this, &UsbRepairWorker::onFsckTimeout);

    QString command;
    if (fsType == "vfat") {
        command = "fsck.fat -a -w " + devicePath;
        m_currentProcess->start("fsck.fat", { "-a", "-w", devicePath });
    } else if (fsType == "exfat") {
        command = "fsck.exfat -p " + devicePath;
        m_currentProcess->start("fsck.exfat", { "-p", devicePath });
    } else if (fsType == "ntfs") {
        command = "ntfsfix " + devicePath;
        m_currentProcess->start("ntfsfix", { devicePath });
    } else if (fsType == "ext4") {
        command = "e2fsck -p " + devicePath;
        m_currentProcess->start("e2fsck", { "-p", devicePath });
    }
    fmInfo() << "UsbRepairWorker: command:" << command;
}
