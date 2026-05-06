// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef INDEXTASK_H
#define INDEXTASK_H

#include "service_textindex_global.h"
#include "utils/taskstate.h"
#include "textindexcreator/textindexcreatorservice.h"

#include <QObject>
#include <QString>

SERVICETEXTINDEX_BEGIN_NAMESPACE

struct HandlerResult
{
    bool success { false };
    bool interrupted { false };
    bool useAnything { false };
    bool fatal { false };
};

class IndexTask : public QObject
{
    Q_OBJECT
public:
    enum class Type {
        Unknow,
        Create,   // 创建索引
        Update,   // 更新索引
        CreateFileList,   // 基于文件列表创建索引
        UpdateFileList,   // 基于文件列表更新索引
        RemoveFileList,   // 基于文件列表删除索引
        MoveFileList   // 基于文件移动列表更新索引路径
    };
    Q_ENUM(Type)

    enum class Status {
        NotStarted,
        Running,
        Finished,
        Failed
    };
    Q_ENUM(Status)

    explicit IndexTask(Type type, const QString &path, TextIndexCreatorService *service,
                       const QStringList &fileList, const QHash<QString, QString> moveFiles, QObject *parent = nullptr);
    ~IndexTask();

    void start();
    void stop();
    bool isRunning() const;

    QString taskPath() const;
    Type taskType() const;
    Status status() const;

    bool isIndexCorrupted() const;
    void setIndexCorrupted(bool corrupted);

    bool silent() const;
    void setSilent(bool newSilent);

    QString errorString() const {
        return m_error;
    }

Q_SIGNALS:
    void progressChanged(SERVICETEXTINDEX_NAMESPACE::IndexTask::Type type, qint64 count, qint64 total);
    void finished(SERVICETEXTINDEX_NAMESPACE::IndexTask::Type type, SERVICETEXTINDEX_NAMESPACE::HandlerResult result);

private:
    void throttleCpuUsage();
    bool doTask();
    void onProgressChanged(qint64 count, qint64 total);
    void setupTextIndexCreatorServiceConnections();
    bool startTaskViaIPC();
    bool startFileListTaskViaIPC(IndexTask::Type type, const QStringList &fileList, bool silent);
    bool startFileMoveTaskViaIPC(const QHash<QString, QString> &movedFiles, bool silent);

    Type m_type;
    QString m_path;
    QString m_indexPath;
    QString m_currentRequestId;
    QString m_error;
    QStringList m_currentFileList;
    QHash<QString, QString> m_currentMovedFiles;
    TextIndexCreatorService *m_extractorService { nullptr };
    Status m_status { Status::NotStarted };
    TaskState m_state;
    bool m_indexCorrupted { false };
    bool m_silent { false };

    static QString typeToString(IndexTask::Type type);
    static IndexTask::Type StringToType(const QString &typeStr);
};

SERVICETEXTINDEX_END_NAMESPACE

Q_DECLARE_METATYPE(SERVICETEXTINDEX_NAMESPACE::IndexTask::Type)
Q_DECLARE_METATYPE(SERVICETEXTINDEX_NAMESPACE::HandlerResult)

#endif   // INDEXTASK_H
