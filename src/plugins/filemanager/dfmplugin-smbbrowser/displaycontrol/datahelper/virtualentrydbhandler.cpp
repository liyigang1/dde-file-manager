// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "virtualentrydbhandler.h"
#include "displaycontrol/info/protocolvirtualentryentity.h"
#include "displaycontrol/utilities/protocoldisplayutilities.h"

#include <dfm-base/dfm_global_defines.h>
#include <dfm-base/base/application/application.h>
#include <dfm-base/base/application/settings.h>
#include <dfm-base/base/standardpaths.h>

#include <dfm-io/dfmio_utils.h>
#include <unistd.h>

DPSMBBROWSER_USE_NAMESPACE
DFMBASE_USE_NAMESPACE

VirtualEntryDbHandler *VirtualEntryDbHandler::instance()
{
    // 静态变量再堆上分配，最后析构不会有顺序问题
    static VirtualEntryDbHandler *ins = new VirtualEntryDbHandler;
    return ins;
}

VirtualEntryDbHandler::~VirtualEntryDbHandler()
{
    if (handler)
        delete handler;
    handler = nullptr;
}

void VirtualEntryDbHandler::clearData()
{
    Q_ASSERT(handler);
    bool ok = handler->dropTable<VirtualEntryData>();
    fmDebug() << "clear all virtual entry:" << ok;
}

void VirtualEntryDbHandler::clearData(const QString &stdPath)
{
    Q_ASSERT(handler);

    VirtualEntryData data;
    data.setKey(stdPath);
    bool ok = handler->remove<VirtualEntryData>(data) ;
    fmDebug() << "remove virtual entry:" << ok << stdPath;
}

void VirtualEntryDbHandler::removeData(const QString &stdPath)
{
    Q_ASSERT(handler);

    const auto &field = Expression::Field<VirtualEntryData>;
    handler->remove<VirtualEntryData>(field("key") == stdPath);

    // if last share of host is removed, remove the host entry from db.
    QStringList allSeperatedItem;
    allProtocolIDs(nullptr, &allSeperatedItem);

    const QString &smbHost = protocol_display_utilities::getSmbHostPath(stdPath);
    bool notLast = std::any_of(allSeperatedItem.cbegin(), allSeperatedItem.cend(),
                               [smbHost](const QString &smb) { return smb.startsWith(smbHost + "/"); });
    if (!notLast) {
        handler->remove<VirtualEntryData>(field("key") == smbHost);
        fmDebug() << "remove host entry:" << smbHost;
    }
}

void VirtualEntryDbHandler::saveAggregatedAndSperated(const QString &stdSmb, const QString &displayName)
{
    // seperate entry
    VirtualEntryData data(stdSmb);
    data.setDisplayName(displayName);
    {
        QString key(stdSmb);
        while (key.endsWith("/"))
            key.chop(1);
        static QString *kRecordFilePath = new QString(QString("/tmp/dfm_smb_mount_%1.ini").arg(getuid()));
        static QString *kRecordGroup = new QString("defaultSmbPath");
        static QRegularExpression *kRegx = new QRegularExpression{ "/|\\.|:" };
        key = key.replace(*kRegx, "_");
        QSettings sets(*kRecordFilePath, QSettings::IniFormat);
        data.setTargetPath(sets.value(QString("%1/%2").arg(*kRecordGroup).arg(key), "").toString());
    }
    saveData(data);
    data.setTargetPath("");

    data.setKey(protocol_display_utilities::getSmbHostPath(stdSmb));
    data.setDisplayName(data.getHost());
    saveData(data);
}

void VirtualEntryDbHandler::saveProtocolData(const QString &stdPath, const QString &displayName)
{
    for (const auto &cached : allProtocolIDs()) {
        QUrl cachedUrl(cached);
        QUrl newUrl(stdPath);
        if (cachedUrl.host() == newUrl.host()
            && cachedUrl.scheme() == newUrl.scheme()) {
            removeData(cached);
        }
    }

    VirtualEntryData data(stdPath);
    data.setDisplayName(displayName);
    // FTP/SFTP不需要targetPath处理，保持为空
    saveData(data);
}

void VirtualEntryDbHandler::saveData(const VirtualEntryData &data)
{
    Q_ASSERT(handler);

    createTable();
    bool inserted = handler->insert<VirtualEntryData>(data, true) >= 0;
    if (!inserted) {
        // do update.
        const auto &field = Expression::Field<VirtualEntryData>;
        handler->update<VirtualEntryData>(field("targetPath") = data.getTargetPath(),
                                          field("key") == data.getKey());
    }
}

QList<QSharedPointer<VirtualEntryData>> VirtualEntryDbHandler::virtualEntries()
{
    Q_ASSERT(handler);

    auto ret = handler->query<VirtualEntryData>().toBeans();
    fmDebug() << "query all virtual entries:" << ret.count();
    return ret;
}

bool VirtualEntryDbHandler::hasOfflineEntry(const QString &stdSmb)
{
    const auto &allOfflined = allProtocolIDs();
    return allOfflined.contains(stdSmb);
}

QStringList VirtualEntryDbHandler::allProtocolIDs(QStringList *aggregated, QStringList *seperated)
{
    auto allEntries = virtualEntries();
    QStringList lst;
    std::for_each(allEntries.cbegin(), allEntries.cend(),
                  [&](const QSharedPointer<VirtualEntryData> data) {
                      lst.append(data->getKey());
                      if (aggregated && data->getHost() == data->getDisplayName())
                          aggregated->append(data->getKey());
                      if (seperated && data->getHost() != data->getDisplayName())
                          seperated->append(data->getKey());
                  });
    return lst;
}

QString VirtualEntryDbHandler::getDisplayNameOf(const QUrl &entryUrl)
{
    QString path = entryUrl.path();
    path.remove("." + QString(kVEntrySuffix));   // ==> smb://1.2.3.4/hello/

    Q_ASSERT(handler);
    const auto &field = Expression::Field<VirtualEntryData>;
    auto data = handler->query<VirtualEntryData>().where(field("key") == path).toBean();
    if (data)
        return data->getDisplayName();

    QUrl u(path);
    return u.host();
}

QString VirtualEntryDbHandler::getFullProtocolPath(const QString &stdPath)
{
    Q_ASSERT(handler);
    const auto &field = Expression::Field<VirtualEntryData>;
    auto data = handler->query<VirtualEntryData>().where(field("key") == stdPath + "/").toBean();
    if (data)
        return stdPath + "/" + data->getTargetPath();
    return stdPath;
}

QString VirtualEntryDbHandler::getQueryString(const QString &stdPath)
{
    Q_ASSERT(handler);
    const auto &field = Expression::Field<VirtualEntryData>;
    auto data = handler->query<VirtualEntryData>().where(field("key") == stdPath).toBean();
    if (data)
        return data->getQueryString();
    return "";
}

bool VirtualEntryDbHandler::checkDbExists()
{
    using namespace dfmio;
    const auto &dbPath = DFMUtils::buildFilePath(StandardPaths::location(StandardPaths::kApplicationConfigPath).toLocal8Bit(),
                                                 "/deepin/dde-file-manager/database",
                                                 nullptr);

    QDir dir(dbPath);
    if (!dir.exists())
        dir.mkpath(dbPath);

    const auto &dbFilePath = DFMUtils::buildFilePath(dbPath.toLocal8Bit(),
                                                     Global::DataBase::kDfmDBName,
                                                     nullptr);
    handler = new SqliteHandle(dbFilePath);
    QSqlDatabase db { SqliteConnectionPool::instance().openConnection(dbFilePath) };
    if (!db.isValid() || db.isOpenError()) {
        fmWarning() << "The database is invalid! open error";
        return false;
    }
    db.close();

    // 检查并升级数据库
    checkAndUpgradeDatabase();

    return true;
}

bool VirtualEntryDbHandler::checkAndUpgradeDatabase()
{
    // 检查是否存在version表
    if (!hasVersionTable()) {
        createVersionTable();
        setDatabaseVersion(1);
    }

    int currentVersion = getDatabaseVersion();
    const int targetVersion = 2;   // 新版本支持queryString

    if (currentVersion < targetVersion) {
        // 升级到版本2：添加queryString字段
        QString sql = "ALTER TABLE virtual_entry_data ADD COLUMN queryString TEXT DEFAULT ''";

        if (!handler->excute(sql)) {
            fmWarning() << "Failed to add queryString column";
            return false;
        }

        setDatabaseVersion(targetVersion);
        fmInfo() << "Database upgraded to version" << targetVersion;
    }

    return true;
}

bool VirtualEntryDbHandler::hasVersionTable()
{
    QString sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='db_version'";
    bool hasTable = false;

    handler->excute(sql, [&hasTable](QSqlQuery *query) {
        if (query && query->next()) {
            hasTable = true;
        }
    });

    return hasTable;
}

void VirtualEntryDbHandler::createVersionTable()
{
    QString sql = "CREATE TABLE IF NOT EXISTS db_version (version INTEGER PRIMARY KEY)";
    handler->excute(sql);
}

int VirtualEntryDbHandler::getDatabaseVersion()
{
    QString sql = "SELECT version FROM db_version ORDER BY version DESC LIMIT 1";
    int version = 1;   // 默认版本

    handler->excute(sql, [&version](QSqlQuery *query) {
        if (query && query->next()) {
            version = query->value(0).toInt();
        }
    });

    return version;
}

void VirtualEntryDbHandler::setDatabaseVersion(int version)
{
    QString sql = QString("INSERT OR REPLACE INTO db_version (version) VALUES (%1)").arg(version);
    handler->excute(sql);
}

bool VirtualEntryDbHandler::createTable()
{
    Q_ASSERT(handler);

    return handler->createTable<VirtualEntryData>(SqliteConstraint::primary("key"),
                                                  SqliteConstraint::unique("key"));
}

VirtualEntryDbHandler::VirtualEntryDbHandler(QObject *parent)
    : QObject(parent)
{
    fmDebug() << "start checking db info";
    checkDbExists();
    fmDebug() << "end checking db info";
    fmDebug() << "start checking db struct";
    checkAndUpdateTable();
    fmDebug() << "end checking db struct";
}

void VirtualEntryDbHandler::checkAndUpdateTable()
{
    Q_ASSERT(handler);
    QString tableName = SqliteHelper::tableName<VirtualEntryData>();

    auto alterTable = [=] {
        bool ret = handler->excute(QString("ALTER TABLE %1 ADD COLUMN targetPath TEXT")
                                           .arg(tableName));
        fmInfo() << "alter table: " << ret;
    };
    handler->excute(QString("PRAGMA table_info(%1)").arg(tableName), [=](QSqlQuery *query) {
        while (query->next()) {
            if (query->value(1).toString() == "targetPath")
                return;
        }
        alterTable();
    });
}
