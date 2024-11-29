#include "dbusfilemonitorjob.h"
#include "dbusfilemonitor.h"
#include "filemonitoradaptor.h"

#include <QDebug>

DDP_FILEMONITOR_BEGIN_NAMESPACE

DbusFileMonitorJob::DbusFileMonitorJob(QObject *parent) : QObject (parent)
{

}

DbusFileMonitorJob::~DbusFileMonitorJob()
{
    if(m_fileMonitor)
        m_fileMonitor->deleteLater();
    if(m_fileMonitorAdaptor)
        m_fileMonitorAdaptor->deleteLater();
    qInfo() << "delete DbusFileMonitorJob";
}

void DbusFileMonitorJob::initDbusConnection()
{
    qInfo() << "startWork !";
    disConnectDbus();
    //连接Dubs
    QDBusConnection connection = QDBusConnection::sessionBus();
    if(!m_fileMonitor)
        m_fileMonitor = new DbusFileMonitor;

    if(!m_fileMonitorAdaptor)
        m_fileMonitorAdaptor = new FilemonitorAdaptor(m_fileMonitor);

    //注册Dbus服务
    if (!connection.registerService(DesktopFileMonitorServiceName)) {
        qDebug() << "error:" << connection.lastError().message();
        return ;
    }
    //注册Dbus对象
    connection.registerObject(DesktopFileMonitorServicePath, m_fileMonitor);
}

void DbusFileMonitorJob::disConnectDbus()
{
    QDBusConnection connection = QDBusConnection::sessionBus();

    connection.unregisterObject(DesktopFileMonitorServicePath);
    connection.unregisterService(DesktopFileMonitorServiceName);
    connection.disconnectFromBus(connection.name());
}

DDP_FILEMONITOR_END_NAMESPACE
