// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VIRTUALENTRYDBHANDLER_H
#define VIRTUALENTRYDBHANDLER_H

#include "dfmplugin_smbbrowser_global.h"
#include "virtualentrydata.h"

#include <dfm-base/base/db/sqlitehandle.h>

#include <QObject>
#include <QList>

DPSMBBROWSER_BEGIN_NAMESPACE

/*
 * The helper store datas like this.
 * it's easy to reuse it in ftp offline entry
 *  | key                      | protocol | host    | port | displayName      |
 *  | smb://1.2.3.4:1234/share | smb      | 1.2.3.4 | 1234 | share on 1.2.3.4 |
 *  | smb://2.3.4.5/share      | smb      | 2.3.4.5 | -1   | share on 2.3.4.5 |
 *  | ftp://3.4.5.6            | ftp      | 3.4.5.6 | -1   | name on 3.4.5.6  |
 */

class VirtualEntryDbHandler : public QObject
{
    Q_OBJECT

public:
    static VirtualEntryDbHandler *instance();
    ~VirtualEntryDbHandler();

    void clearData();
    void clearData(const QString &stdPath);
    void removeData(const QString &stdPath);
    void saveAggregatedAndSperated(const QString &stdSmb, const QString &displayName);
    void saveProtocolData(const QString &stdPath, const QString &displayName);
    void saveData(const VirtualEntryData &data);

    bool hasOfflineEntry(const QString &stdSmb);
    QStringList allProtocolIDs(QStringList *aggregated = nullptr, QStringList *seperated = nullptr);
    QString getDisplayNameOf(const QUrl &entryUrl);
    QString getFullProtocolPath(const QString &stdSmb);
    QString getQueryString(const QString &stdPath);
    QList<QSharedPointer<VirtualEntryData>> virtualEntries();

protected:
    bool checkDbExists();
    bool createTable();
    bool checkAndUpgradeDatabase();
    int getDatabaseVersion();
    void setDatabaseVersion(int version);
    bool hasVersionTable();
    void createVersionTable();

private:
    explicit VirtualEntryDbHandler(QObject *parent = nullptr);

    void checkAndUpdateTable();

    DFMBASE_NAMESPACE::SqliteHandle *handler { nullptr };
};

DPSMBBROWSER_END_NAMESPACE

#endif   // VIRTUALENTRYDBHANDLER_H
