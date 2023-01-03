/*
 * Copyright (C) 2021 Uniontech Software Technology Co., Ltd.
 *
 * Author:     liyigang<liyigang@uniontech.com>
 *
 * Maintainer: max-lv<lvwujun@uniontech.com>
 *             lanxuesong<lanxuesong@uniontech.com>
 *             xushitong<xushitong@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef TASKDIALOG_H
#define TASKDIALOG_H

#include "dfm-base/dfm_base_global.h"
#include "dfm-base/interfaces/abstractjobhandler.h"

#include <DDialog>
#include <DTitlebar>

#include <QObject>
#include <QtDBus/QtDBus>

class QListWidget;
class QListWidgetItem;
class QMutex;

DWIDGET_USE_NAMESPACE
namespace dfmbase {
class TaskWidget;
class TaskDialog : public DAbstractDialog
{
    Q_OBJECT

public:
    explicit TaskDialog(QObject *parent = nullptr);
    ~TaskDialog() override;
    void addTask(const JobHandlePointer taskHandler);
    void initUI();
    void blockShutdown();
    void addTaskWidget(const JobHandlePointer taskHandler, TaskWidget *wid);
    void setTitle(int taskCount);

Q_SIGNALS:
    /*!
     * \brief closed 当前进度窗口关闭时，发送关闭信号
     */
    void closed();
private Q_SLOTS:
    void adjustSize();
    void moveYCenter();
    void removeTask(const JobInfoPointer info);

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QListWidget *taskListWidget { nullptr };
    QMap<JobHandlePointer, QListWidgetItem *> taskItems;
    DTitlebar *titlebar { nullptr };
    QDBusReply<QDBusUnixFileDescriptor> replyBlokShutDown;
    QMutex *addTaskMutex { nullptr };
    QMutex *adjustSizeMutex { nullptr };
    static int kMaxHeight;
};

}

#endif   // TASKDIALOG_H
