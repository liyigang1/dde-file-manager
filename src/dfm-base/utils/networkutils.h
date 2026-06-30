// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NETWORKUTILS_H
#define NETWORKUTILS_H

#include <dfm-base/dfm_base_global.h>

#include <QObject>
#include <QString>
#include <QMap>
#include <QDateTime>
#include <QMutex>

#include <functional>
#include <libmount.h>

namespace dfmbase {

class NetworkUtils : public QObject
{
    Q_OBJECT

public:
    static NetworkUtils *instance();

    bool checkNetConnection(const QString &host, const QString &port, int msecs = 1000, const bool useCache = true);
    bool checkNetConnection(const QString &host, QStringList ports, int msecs = 1000);
    void doAfterCheckNet(const QString &host, const QStringList &ports,
                         std::function<void(bool)> callback = nullptr, int msecs = 3000);
    bool parseIp(const QString &mpt, QString &ip, QString &port);
    bool parseIp(const QString &mpt, QString &ip, QStringList &ports);
    bool checkFtpOrSmbBusy(const QUrl &url);

    // TTL 缓存：避免短期内对同一 host:port 重复 TCP 连接
    struct NetCacheEntry {
        bool busy { false };
        QDateTime timestamp;
    };
    static constexpr int kNetCacheTTLMs { 500 };   // 500 ms

    static QString makeCacheKey(const QString &host, const QString &port);
    NetCacheEntry getFromCache(const QString &host, const QString &port) const;
    void updateCache(const QString &host, const QString &port, bool busy);
    void clearCache();

private:
    QString hexIpToString(const QString& hexIp);
    bool getHostAndPortByReadNet(QString &host, QStringList &ports);
    mutable QMutex cacheMutex;
    mutable QMap<QString, NetCacheEntry> netCache;

protected:
    explicit NetworkUtils(QObject *parent = nullptr);
};

}

#endif   // NETWORKUTILS_H
