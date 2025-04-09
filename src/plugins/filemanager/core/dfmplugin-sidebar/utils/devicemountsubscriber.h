// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DEVICEMOUNTSUBSCRIBER_H
#define DEVICEMOUNTSUBSCRIBER_H

#include "dfmplugin_sidebar_global.h"

#include <QObject>
#include <QMap>
#include <QUrl>
#include <QTimer>
#include <QDateTime>
#include <functional>

DPSIDEBAR_BEGIN_NAMESPACE

/**
 * @brief 设备挂载事件订阅管理类
 * 
 * 用于实现发布-订阅模式，处理设备挂载完成后的回调
 * 
 * @example 使用示例：
 * 
 * 1. 订阅设备挂载完成事件
 * ```cpp
 * // 订阅特定设备的挂载事件
 * QUrl deviceUrl("device:///dev/sdb1");
 * int subscriptionId = DeviceMountSubscriber::instance()->subscribe(
 *     deviceUrl, 
 *     [](const QUrl &mountedUrl) {
 *         qDebug() << "Device mounted at:" << mountedUrl;
 *         // 处理挂载完成后的操作
 *     }
 * );
 * ```
 * 
 * 2. 通知设备挂载完成
 * ```cpp
 * // 设备挂载完成后，通知所有订阅者
 * QUrl deviceUrl("device:///dev/sdb1");
 * QUrl mountedUrl("file:///media/user/USBDISK");
 * DeviceMountSubscriber::instance()->notifyMountFinished(deviceUrl, mountedUrl);
 * ```
 * 
 * 3. 取消订阅（可选，过期订阅会自动清理）
 * ```cpp
 * DeviceMountSubscriber::instance()->unsubscribe(subscriptionId);
 * ```
 */
class DeviceMountSubscriber : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 获取单例实例
     * @return DeviceMountSubscriber* 单例实例指针
     */
    static DeviceMountSubscriber *instance();
    
    /**
     * @brief 订阅设备挂载完成事件
     * @param deviceUrl 设备URL
     * @param callback 挂载完成后的回调函数
     * @return int 订阅ID，可用于取消订阅
     */
    int subscribe(const QUrl &deviceUrl, std::function<void(const QUrl&)> callback);
    
    /**
     * @brief 取消订阅
     * @param subscriptionId 订阅ID
     */
    void unsubscribe(int subscriptionId);
    
    /**
     * @brief 通知设备挂载已完成
     * @param deviceUrl 设备URL
     * @param mountedUrl 挂载后的目标URL
     */
    void notifyMountFinished(const QUrl &deviceUrl, const QUrl &mountedUrl);
    
private:
    /**
     * @brief 构造函数，设为私有以实现单例模式
     * @param parent 父对象指针
     */
    DeviceMountSubscriber(QObject *parent = nullptr);
    
    /**
     * @brief 订阅信息结构体
     */
    struct Subscription {
        int id;                                  ///< 订阅ID
        QUrl deviceUrl;                          ///< 设备URL
        std::function<void(const QUrl&)> callback; ///< 回调函数
        QDateTime timestamp;                     ///< 创建时间戳
    };
    
    QMap<int, Subscription> subscriptions;       ///< 订阅映射表
    int nextSubscriptionId = 0;                  ///< 下一个订阅ID
    QTimer cleanupTimer;                         ///< 清理定时器
    
    /**
     * @brief 清理过期的订阅
     */
    void cleanupExpiredSubscriptions();
};

DPSIDEBAR_END_NAMESPACE

#endif // DEVICEMOUNTSUBSCRIBER_H 