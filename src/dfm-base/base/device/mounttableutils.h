// SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MOUNTTABLEUTILS_H
#define MOUNTTABLEUTILS_H

#include "private/mounttableutils_p.h"

#include <dfm-base/dfm_base_global.h>

#include <QObject>


namespace dfmbase {

/*!
 * \brief The MountTableUtilsPrivate class
 * this class provide MountTableUtils data
 */
class MountTableUtils : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(MountTableUtils)
public:
    virtual ~MountTableUtils() override {}
    /**
     * @brief instance 获取MountTableUtils
     * @return MountTableUtils的指针
     */
    static MountTableUtils *instance();
    /**
     * @brief isSharePotocolMount 根据url(只能是本地文件)去获取所有的共享挂载点来判断当前url是否是网络共享挂载
     * @param url 文件的路径
     * @return 是否是网络共享文件挂载
     */
    bool isSharePotocolMount(const QUrl &url);
    // if network mount，get network mount
    /**
     * @brief mountHostInfo 获取所有mounts表中挂载存在host的挂载点和host
     * @return QMap<QString, QString> key : mountpoint,value : host:port
     */
    QMap<QString, QString> mountHostInfo();

private:
    explicit MountTableUtils(QObject *parent = nullptr);

private:
    QScopedPointer<MountTableUtilsPrivate> d { nullptr };
};

}

#endif   // MOUNTTABLEUTILS_H
