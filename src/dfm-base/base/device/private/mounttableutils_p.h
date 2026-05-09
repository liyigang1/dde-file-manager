// SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MOUNTTABLEUTILS_P_H
#define MOUNTTABLEUTILS_P_H

#include <dfm-base/dfm_base_global.h>

#include <QObject>
#include <QMutex>
#include <QSet>

#include <libmount.h>

namespace dfmbase {

namespace {
inline constexpr int kCacheTimeElapsed { 300 };
inline constexpr char kKeyShareFileProtocol[] { "sharedFileProtocol" };
}

/*!
 * \brief The MountTableUtilsPrivate class
 * this class provide some util functions.
 */
class MountTableUtilsPrivate : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(MountTableUtilsPrivate)
    friend class MountTableUtils;
public:
    virtual ~MountTableUtilsPrivate() override {}
    /**
     * @brief isSharePotocolMount 根据url(只能是本地文件)去获取所有的共享挂载点来判断当前url是否是网络共享挂载
     * @param url 文件的路径
     * @return 是否是网络共享文件挂载
     */
    bool isSharePotocolMount(const QString &path);
    /**
     * @brief allMountsHostInfo 获取所有含有host挂载点和host的
     * @return 返回所有含有host挂载点和host的
     */
    QMap<QString, QString> allMountsHostInfo();
    /**
     * @brief dlnfsMountPoints 获取所有 dlnfs 挂载点缓存
     * @return dlnfs 挂载点集合
     */
    QSet<QString> dlnfsMountPoints();

    QMutex shareProtocolMutex; // 读取mount的锁
    QStringList deflautProtocol; // 默认的共享网络协议
    QSet<QString> shareProtocolmountPointCaches; // 共享网络协议的挂载点缓存
    QSet<QString> dlnfsMountPointCaches; // dlnfs 挂载点缓存
    qint64 mountReadTime { 0 }; // 读取mounts表的时间
    QMap<QString, QString> mountHostCaches; // 所有含有host挂载点和host的缓存

private:
    explicit MountTableUtilsPrivate(QObject *parent = nullptr);
    /**
     * @brief mountPointCacheContains path是否再共享网络协议的挂载点上
     * @param path
     * @return
     */
    bool mountPointCacheContains(const QString &path);
    /**
     * @brief ipByMountOption 根据挂载的options获取host
     * @param fs
     * @return host
     */
    QString ipByMountOption(libmnt_fs * fs);
    /**
     * @brief ipByMountScource 根据挂载的原始路径获取host
     * @param fs
     * @return host
     */
    QString ipByMountScource(libmnt_fs * fs);
    /**
     * @brief readMounts 读取mounts表的数据
     */
    void readMounts();
};

}

#endif   // MOUNTTABLEUTILS_P_H
