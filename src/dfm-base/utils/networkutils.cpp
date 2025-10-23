// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "networkutils.h"
#include <dfm-base/dfm_log_defines.h>
#include <dfm-base/base/configs/dconfig/dconfigmanager.h>

#include <QtConcurrent>
#include <QFutureWatcher>
#include <QTcpSocket>
#include <QNetworkProxy>

#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>

using namespace dfmbase;

static constexpr char kSmbPort[] { "445" };
static constexpr char kSmbPortOther[] { "139" };
static constexpr char kFtpPort[] { "21" };
static constexpr char kSftpPort[] { "22" };

NetworkUtils *NetworkUtils::instance()
{
    // 静态变量再堆上分配，最后析构不会有顺序问题
    static NetworkUtils *netWorkUtils = new NetworkUtils;
    return netWorkUtils;
}

bool NetworkUtils::checkNetConnection(const QString &host, const QString &port, int msecs)
{
    if (host.isEmpty())
        return true;

    auto checkNet = DConfigManager::instance()->value("org.deepin.dde.file-manager.mount",
                                                      "checkNetworkAccessable",
                                                      false)
                            .toBool();

    if (!checkNet) {
        qCInfo(logDFMBase) << "Skip network check." << host << port;
        return true;
    }

    qCDebug(logDFMBase) << "net work check host = " << host << ", port = " << port << " !!!";

    QTcpSocket conn;
    conn.connectToHost(host, port.toUShort());
    bool connected = conn.waitForConnected(msecs);
    conn.close();
    // 如果系统设置了代理，那么QTcpSocket会使用代理去连接目标host，代理可能不能访问目标host
    // 在QTcpSocket使用代理不能访问目标host的情况下，将QTcpSocket设置为不使用代理再次连接host，检查能否访问目标host
    if (!connected) {
        // 检查系统代理设置
        QNetworkProxy proxy = QNetworkProxy::applicationProxy();
        // 如果系统代理手动设置了http或者https，设置忽略的ip后面带有“,”结尾，这里检查QNetworkProxy的类型还是NoProxy
        // 所以去掉再次设置不要代理去检查一次
        conn.setProxy(QNetworkProxy::NoProxy);
        conn.connectToHost(host, port.toUShort());
        connected = conn.waitForConnected(msecs);
        conn.close();
    }
    return connected;
}

bool NetworkUtils::checkNetConnection(const QString &host, QStringList ports, int msecs)
{
    static QString lastPort;
    if (!lastPort.isEmpty() && ports.contains(lastPort)) {
        ports.removeOne(lastPort);
        if (checkNetConnection(host, lastPort, msecs))
            return true;
    }

    for (auto const &port : ports) {
        if (checkNetConnection(host, port, msecs)) {
            lastPort = port;
            return true;
        }
    }

    return false;
}

void NetworkUtils::doAfterCheckNet(const QString &host, const QStringList &ports, std::function<void(bool)> callback, int msecs)
{
    QFutureWatcher<bool> *watcher = new QFutureWatcher<bool>();
    QObject::connect(watcher, &QFutureWatcher<bool>::finished, [callback, watcher]() {
        if (callback)
            callback(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([host, ports, msecs]() {
        for (const auto &port : ports) {
            qApp->processEvents();
            if (NetworkUtils::instance()->checkNetConnection(host, port, msecs))
                return true;
        }
        return false;
    }));
}

bool NetworkUtils::parseIp(const QString &mpt, QString &ip, QString &port)
{
    QString s(mpt);
    static QRegularExpression *gvfsPref = new QRegularExpression { "(^/run/user/\\d+/gvfs/|^/root/\\.gvfs/)" };
    static QRegularExpression *cifsMptPref = new QRegularExpression { "^/media/[\\s\\S]*/smbmounts/" };   // TODO(xust) smb mount point may be changed.

    if (s.contains(*gvfsPref)) {
        s.remove(*gvfsPref);
    } else if (s.contains(*cifsMptPref)) {
        s.remove(*cifsMptPref);
    } else {
        auto cifsHost = cifsMountHostInfo();
        for (const auto &mountPoint : cifsHost.keys()) {
            if (mpt.startsWith(mountPoint)) {
                auto hostAndPort = cifsHost.value(mountPoint).split(":");
                if (hostAndPort.isEmpty())
                    continue;
                ip = hostAndPort[0];
                port = hostAndPort.count() > 1 ? hostAndPort[1] : "";
                return true;
            }
        }
        return false;
    }

    // s = ftp:host=1.2.3.4  smb-share:server=1.2.3.4,share=draw
    bool isFtp = s.startsWith("ftp");
    bool isSftp = s.startsWith("sftp");
    bool isSmb = s.startsWith("smb");

    if (!isFtp && !isSftp && !isSmb)
        return false;

    // ftp:host=1.2.3.4,port=123  smb-share:port=321,server=1.2.3.4,share=draw
    static QRegularExpression *hostAndPortRegx = new QRegularExpression(R"(([:,]port=(?<port0>\d*))?[,:](server|host)=(?<host>[^/:,]+)(,port=(?<port1>\d*))?)");
    // --------------------------------------------------forward PORT ---|--------------------- ip/host ---|--- backward PORT ---|, PORT is optional
    auto match = hostAndPortRegx->match(s);
    if (match.hasMatch()) {
        auto capturedPort = match.captured("port0");
        if (capturedPort.isEmpty())
            capturedPort = match.captured("port1");

        if (!capturedPort.isEmpty())
            port = capturedPort;
        else if (isSmb)
            port = kSmbPort;
        else if (isFtp)
            port = kFtpPort;
        else if (isSftp)
            port = kSftpPort;
        else
            port = kSmbPort;

        ip = match.captured("host");
        return true;
    }

    return false;
}

bool NetworkUtils::parseIp(const QString &mpt, QString &ip, QStringList &ports)
{
    QString port;
    if (parseIp(mpt, ip, port)) {
        if (!ip.isEmpty() && port.isEmpty())
            return cifsMountHostPortInfo(ip, ports);

        ports.append(port);
        if (port == kSmbPort)
            ports.append(kSmbPortOther);
        if (port == kSmbPortOther)
            ports.append(kSmbPort);
        return true;
    }
    return false;
}

bool NetworkUtils::checkFtpOrSmbBusy(const QUrl &url)
{
    QString host;
    QStringList ports;
    // 这里host可以解析处理,但是解析不出来ports也是网络远程断开,所以这里判断一下host是空就不是busy
    if (!parseIp(url.path(), host, ports))
        return !host.isEmpty();

    auto busy = !checkNetConnection(host, ports);
    if (busy)
        qCInfo(logDFMBase) << "can not connect url = " << url << " host =  " << host << " port = " << ports;

    return busy;
}

QMap<QString, QString> NetworkUtils::cifsMountHostInfo()
{
    static QMutex mutex;
    static QMap<QString, QString> *table = new QMap<QString, QString>;
    static qint64 curTime = 0;
    QMutexLocker locker(&mutex);
    if (curTime != 0 && QDateTime::currentMSecsSinceEpoch() - curTime < 300)
        return *table;

    curTime = QDateTime::currentMSecsSinceEpoch();

    table->clear();

    libmnt_table *tab { mnt_new_table() };
    libmnt_iter *iter { mnt_new_iter(MNT_ITER_BACKWARD) };

    int ret = mnt_table_parse_mtab(tab, nullptr);
    if (ret != 0) {
        mnt_free_table(tab);
        mnt_free_iter(iter);
        qWarning() << "device: cannot parse mtab" << ret;
        return *table;
    }

    libmnt_fs *fs = nullptr;
    while (mnt_table_next_fs(tab, iter, &fs) == 0) {
        if (!fs)
            continue;

        // use options get ip
        // net work mount must start with //
        QString srcHostAndPort = ipByMountOption(fs);
        if (srcHostAndPort.isEmpty())
            srcHostAndPort = ipByMountScource(fs);

        if (srcHostAndPort.isEmpty())
            continue;

        const QString &mountPath = mnt_fs_get_target(fs);
        table->insert(mountPath, srcHostAndPort);
    }

    mnt_free_table(tab);
    mnt_free_iter(iter);
    return *table;
}

QString NetworkUtils::hexIpToString(const QString& hexIp)
{
    bool ok;
    quint32 ip= hexIp.toUInt(&ok, 16);
    return QString("%4.%3.%2.%1")
    .arg((ip >> 24) & 0xFF)
    .arg((ip >> 16) & 0xFF)
    .arg((ip >> 8) & 0xFF)
    .arg(ip & 0xFF);
}

QString NetworkUtils::ipByMountOption(libmnt_fs *fs)
{
    //rw,relatime,vers=4.2,rsize=1048576,wsize=1048576,namlen=255,hard,proto=tcp,
    //timeo=600,retrans=2,sec=sys,clientaddr=10.8.12.43,local_lock=none,addr=10.8.12.25
    QString ops = mnt_fs_get_options(fs);
    auto startAdd = ops.startsWith("addr=");
    if (!startAdd && !ops.contains(",addr="))
        return "";
    ops = ops.mid(ops.indexOf(startAdd ? "addr=" : ",addr=")).replace(startAdd ? "addr=" : ",addr=", "");
    auto index = ops.indexOf(",");
    if (index < 0)
        return ops;
    return ops.left(index);
}

QString NetworkUtils::ipByMountScource(libmnt_fs *fs)
{
    QString srcHostAndPort = mnt_fs_get_source(fs);
    if (!srcHostAndPort.startsWith("//"))
        return "";

    srcHostAndPort = srcHostAndPort.mid(2);
    srcHostAndPort = srcHostAndPort.left(srcHostAndPort.indexOf("/"));
    return srcHostAndPort;
}

bool NetworkUtils::cifsMountHostPortInfo(QString &host, QStringList &ports)
{
    if (host.isEmpty())
        return false;

    QFile file("/proc/net/tcp");
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)){
        qCDebug(logDFMBase) << "Cannot open /proc/net /tcp";
        return {};
    }

    QTextStream in(&file);
    // 跳过标题行
    in.readLine();

    while(!in.atEnd()){
        QString line = in.readLine();
        QStringList fields = line.split(QRegExp("\\s+"), QString::SplitBehavior::SkipEmptyParts);
        if(fields.size()< 12)
            continue;

        //解析远程地址和端口
        QStringList remote = fields[2].split(":");
        // fields[3]是状态, 0x01 是"ESTABLISHED"状态
        //    case 0x01: return "ESTABLISHED":
        //    case 0x02: return "SYN_SENT";
        //    case 0x03: return "SYN RECY";
        //    case 0x04: return "FIN_WAIT1",
        //    case 0x05: return "FIN_WAIT2",
        //    case 0x06: return "TIME_WAIT";
        //    case 0x07: return
        //    "CLOSE";
        //    case 0x08: return "CLOSE_WAIT",
        //    case 0x09: return
        //    "LAST ACK";
        //    case 0xOA: return "LISTEN";
        //    case
        //    0xOB:
        //    return
        //    "CLOSTNG"
        //    default: return "UNKNOWN".
        if(remote.size() == 2 && hexIpToString(remote[0]) == host && fields[3].toUInt(nullptr, 16) == 0x01){
            auto port = QString::number(remote[1].toUInt(nullptr, 16));
            if (port != "0" && !ports.contains(port))
                ports.append(port);
        }
    }
    return !ports.isEmpty();
}

NetworkUtils::NetworkUtils(QObject *parent)
    : QObject(parent)
{
}
