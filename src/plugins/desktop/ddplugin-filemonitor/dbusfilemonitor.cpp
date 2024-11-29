#include "dbusfilemonitor.h"
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QThread>
DbusFileMonitor::DbusFileMonitor(QObject *parent) : QObject(parent)
{
    m_checker = new QTimer(this);
    m_checker->setInterval(m_checkerInteral);
    connect(m_checker,&QTimer::timeout,this,&DbusFileMonitor::broadcastData);
}

void DbusFileMonitor::broadcastData()
{
    QJsonArray data;
    int limit = m_oneMsgMaxItemCount + 1;
    while (!m_dataList.isEmpty()) {
        limit--;
        if(limit <= 0)
            break;
        QSharedPointer<BoardData> temData = m_dataList.takeFirst();
        QJsonObject obj;
        obj.insert("from",temData->from);
        obj.insert("to",temData->to);
        QJsonArray jsonArray;
        for (const QString &str : temData->fileNames) {
            jsonArray.append(QJsonValue(str));
        }
        obj.insert("fileNames", QJsonValue(jsonArray));

        data.append(obj);
        QThread::msleep(2);
    }

    QJsonDocument doc(data);
    Q_EMIT FilePastDataMonitor(doc.toJson(/*QJsonDocument::Compact*/));
    if(m_dataList.isEmpty()){
        m_checker->stop();
    }
}

void DbusFileMonitor::PrepareSendData(const QString &from, const QString &to, const QStringList &fileNames)
{
    if(from.isEmpty() || to.isEmpty()){
        qInfo()<<"Past barodcast data from or to is empty.";
        qInfo()<<"from = "<<from;
        qInfo()<<"to = "<<to;
        return;
    }
    //当文管传来粘贴数据 且 m_checker未启动，则 启动m_checker周期性从m_dataList广播数据
    if(!m_checker->isActive()){
        m_checker->start();
    }
    QSharedPointer<BoardData> data(new BoardData);
    data->from = from;
    data->to = to;
    data->fileNames = fileNames;
    m_dataList << data;
}
