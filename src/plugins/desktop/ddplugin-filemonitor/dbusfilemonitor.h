#ifndef DBUSFILEMONITOR_H
#define DBUSFILEMONITOR_H

#include <QObject>
#include <QDBusContext>
#include <QDBusConnection>
#include <QDBusServer>
#include <QDBusMessage>
#include <QDBusConnectionInterface>

#define DesktopFileMonitorServiceName          "com.deepin.dde.desktop.filemonitor"
#define DesktopFileMonitorServicePath          "/com/deepin/dde/desktop/filemonitor"
#define DesktopFileMonitorServiceInterface     "com.deepin.dde.desktop.filemonitor"
class QTimer;
class DbusFileMonitor : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", DesktopFileMonitorServiceName)
    struct BoardData{
        QString from;
        QString to;
        QStringList fileNames;
    };
public:
    explicit DbusFileMonitor(QObject *parent = nullptr);
private slots:
    void broadcastData();
public Q_SLOTS:
    void PrepareSendData(const QString &from, const QString &to, const QStringList &fileNames);
Q_SIGNALS:
    void FilePastDataMonitor(const QString &data);
private:
    QList<QSharedPointer<BoardData>> m_dataList;//来自文管的待广播数据
    QTimer* m_checker = nullptr;
    int m_oneMsgMaxItemCount = 100;//每次广播消息内容数组下标的上限。
    int m_checkerInteral = 0;//发送频率。m_checker检查m_dataList的间隔时间，单位：ms
};

#endif // DBUSFILEMONITOR_H
