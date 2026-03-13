// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mounttableutils.h"

#include <dfm-base/dfm_log_defines.h>
#include <dfm-base/base/configs/dconfig/dconfigmanager.h>

#include <QUrl>
#include <QDir>
#include <QDateTime>
#include <QLoggingCategory>

#include <libmount.h>
#include <fstab.h>
#include <sys/stat.h>

using namespace dfmbase;

bool MountTableUtilsPrivate::mountPointCacheContains(const QString &path)
{
    for (const auto &mtp : shareProtocolmountPointCaches) {
        if (path.startsWith(mtp))
            return true;
    }
    return false;
}

bool MountTableUtilsPrivate::isSharePotocolMount(const QString &path)
{
    QMutexLocker locker(&shareProtocolMutex);
    // 再访问时间不操过kCacheTimeElapsed 300ms时,访问缓存
    if (mountReadTime == 0 || QDateTime::currentMSecsSinceEpoch() - mountReadTime > kCacheTimeElapsed) {
        // 重新初始化mount读取时间和挂载点
        mountReadTime = QDateTime::currentMSecsSinceEpoch();
        shareProtocolmountPointCaches.clear();
        readMounts();
    }
    return mountPointCacheContains(path);
}

QMap<QString, QString> MountTableUtilsPrivate::allMountsHostInfo()
{
    QMutexLocker locker(&shareProtocolMutex);
    if (mountReadTime == 0 || QDateTime::currentMSecsSinceEpoch() - mountReadTime > kCacheTimeElapsed) {
        mountReadTime = QDateTime::currentMSecsSinceEpoch();
        mountHostCaches.clear();
        readMounts();
    }
    return mountHostCaches;
}

QString MountTableUtilsPrivate::ipByMountOption(libmnt_fs *fs)
{
    //rw,relatime,vers=4.2,rsize=1048576,wsize=1048576,namlen=255,hard,proto=tcp,
    //timeo=600,retrans=2,sec=sys,clientaddr=10.8.12.43,local_lock=none,addr=10.8.12.25,
    //mountaddr=10.8.12.25,mountport=2048,port=2048
    QString ops = mnt_fs_get_options(fs);
    if (ops.isEmpty() || !ops.contains("addr="))
        return QString();
    QStringList opsList = ops.split(",");
    if (opsList.isEmpty())
        return QString();
    QString host,port;
    for (const auto &op : opsList) {
        if (host.isEmpty() && (op.startsWith("addr=") || op.startsWith("mountaddr="))) {
            host = op.mid(op.indexOf("=") + 1);
        }
        if (port.isEmpty() && (op.startsWith("port=") || op.startsWith("mountport="))) {
            port = op.mid(op.indexOf("=") + 1);
        }
    }
    if (host.isEmpty())
        return QString();

    if (port.isEmpty())
        return host;

    return host + ":" +port;
}

QString MountTableUtilsPrivate::ipByMountScource(libmnt_fs *fs)
{
    QString srcHostAndPort = mnt_fs_get_source(fs);
    if (!srcHostAndPort.startsWith("//")) {
        qCDebug(logDFMBase) << "MountTableUtilsPrivate::ipByMountScource is not start with \"//\"";
        return "";
    }

    srcHostAndPort = srcHostAndPort.mid(2);
    srcHostAndPort = srcHostAndPort.left(srcHostAndPort.indexOf("/"));
    return srcHostAndPort;
}

void MountTableUtilsPrivate::readMounts()
{
    auto shareProtocol = DConfigManager::instance()->value(kMountDConfName, kKeyShareFileProtocol, deflautProtocol).toStringList();
    libmnt_table *tab { mnt_new_table() };
    // mounts访问重后往前迭代(先访问最新的挂载)
    libmnt_iter *iter { mnt_new_iter(MNT_ITER_BACKWARD) };
    int ret = mnt_table_parse_mtab(tab, nullptr);
    if (ret != 0) {
        mnt_free_table(tab);
        mnt_free_iter(iter);
        qCWarning(logDFMBase) << "MountTableUtilsPrivate::isSharePotocolMount device: cannot parse mtab " << ret
                              << strerror(errno);
        return;
    }

    libmnt_fs *fs = nullptr;
    while (mnt_table_next_fs(tab, iter, &fs) == 0) {
        if (!fs)
            continue;
        QString mtp = mnt_fs_get_target(fs);
        QString fsType = mnt_fs_get_fstype(fs);

        // use options get ip
        // net work mount must start with //
        QString srcHostAndPort = ipByMountOption(fs);
        if (srcHostAndPort.isEmpty())
            srcHostAndPort = ipByMountScource(fs);

        // 多个挂载点只是有最近挂载的这个挂载点,或者可以获取到host表示也是网络共享挂载
        if (!shareProtocolmountPointCaches.contains(mtp) && (shareProtocol.contains(fsType) || !srcHostAndPort.isEmpty()))
            shareProtocolmountPointCaches.insert(mtp);

        if (srcHostAndPort.isEmpty())
            continue;

        const QString &mountPath = mnt_fs_get_target(fs);
        // using new mount
        if (!mountHostCaches.contains(mountPath))
            mountHostCaches.insert(mountPath, srcHostAndPort);
    }
    mnt_free_table(tab);
    mnt_free_iter(iter);
}

MountTableUtilsPrivate::MountTableUtilsPrivate(QObject *parent)
    : QObject(parent)
{
    deflautProtocol = QStringList{"cifs", "nfs", "nfs4", "smbfs", "ftp", "sshfs", "webdav", "afp", "ftp", "sftp"};
}

MountTableUtils *MountTableUtils::instance()
{
    static MountTableUtils *mountTableUtilsinstance = new MountTableUtils();
    return mountTableUtilsinstance;
}

bool MountTableUtils::isSharePotocolMount(const QUrl &url)
{
    if (!url.isValid() && !url.isLocalFile()) {
        qCWarning(logDFMBase) << QString("MountTableUtils::isSharePotocolMount url is %1 .. ").arg(url.isValid() ? "not local file" : "unValid")
                              << url;
        return false;
    }
    auto sp = QDir::separator();
    auto path = url.path().endsWith(sp) ? url.path() : url.path() + sp;

    return d->isSharePotocolMount(path);
}

QMap<QString, QString> MountTableUtils::mountHostInfo()
{
    return d->allMountsHostInfo();
}

MountTableUtils::MountTableUtils(QObject *parent)
    : QObject (parent), d(new MountTableUtilsPrivate(this))
{
}
