#ifndef DBUSFILEMONITORJOB_H
#define DBUSFILEMONITORJOB_H

#include "ddplugin_filemonitor_global.h"
#include <QObject>

class DbusFileMonitor;
class FilemonitorAdaptor;
class QTimer;

DDP_FILEMONITOR_BEGIN_NAMESPACE
class DbusFileMonitorJob : public QObject
{
    Q_OBJECT
public:
    explicit DbusFileMonitorJob(QObject *parent = nullptr);
    ~DbusFileMonitorJob() override;
public slots:
    void initDbusConnection();
public:
    void disConnectDbus();
private:
    FilemonitorAdaptor *m_fileMonitorAdaptor = nullptr;
    DbusFileMonitor *m_fileMonitor = nullptr;
};

DDP_FILEMONITOR_END_NAMESPACE

#endif // DBUSFILEMONITORJOB_H
