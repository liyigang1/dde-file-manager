/*
 * Copyright (C) 2021 ~ 2022 Uniontech Software Technology Co., Ltd.
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
#ifndef DOMOVETOTRASHFILESWORKER_H
#define DOMOVETOTRASHFILESWORKER_H

#include "dfm_common_service_global.h"
#include "dfm-base/interfaces/abstractjobhandler.h"
#include "fileoperations/fileoperationutils/fileoperatebaseworker.h"
#include "dfm-base/interfaces/abstractfileinfo.h"

#include <QObject>

#include <dfm-io/core/dfile.h>

class QStorageInfo;

USING_IO_NAMESPACE
DSC_BEGIN_NAMESPACE
DFMBASE_USE_NAMESPACE
class DoMoveToTrashFilesWorker : public FileOperateBaseWorker
{
    friend class MoveToTrashFiles;
    Q_OBJECT
    explicit DoMoveToTrashFilesWorker(QObject *parent = nullptr);

public:
    virtual ~DoMoveToTrashFilesWorker() override;

protected:
    bool doWork() override;
    bool statisticsFilesSize() override;

protected:
    AbstractJobHandler::SupportAction doHandleErrorAndWait(const QUrl &from,
                                                           const AbstractJobHandler::JobErrorType &error,
                                                           const QString &errorMsg = QString());
    bool doMoveToTrash();
    bool checkTrashDirIsReady();
    bool canMoveToTrash(const QString &filePath);
    bool isCanMoveToTrash(const QUrl &url, bool &result);
    bool handleSymlinkFile(const AbstractFileInfoPointer &fileInfo);
    bool handleMoveToTrash(const AbstractFileInfoPointer &fileInfo);
    bool checkFileOutOfLimit(const AbstractFileInfoPointer &fileInfo);
    bool WriteTrashInfo(const AbstractFileInfoPointer &fileInfo, QString &targetPath, bool &result);
    QString getNotExistsTrashFileName(const QString &fileName);
    bool doWriteTrashInfo(const QString &fileBaseName, const QString &path, const QString &time);
    void isInSameDisk(const AbstractFileInfoPointer &fileInfo);
    bool doCopyAndDelete(const AbstractFileInfoPointer &fromInfo, const AbstractFileInfoPointer &toInfo);

private:
    AbstractFileInfoPointer targetFileInfo { nullptr };   // target file information
    QAtomicInteger<qint64> compeleteFilesCount { 0 };   // move to trash success file count
    qint8 isSameDisk { -1 };   // the source file and trash files is in same disk
    QString trashLocalDir;   // the trash file locak dir
    QSharedPointer<QStorageInfo> targetStorageInfo { nullptr };   // target file's device infor
};

DSC_END_NAMESPACE

#endif   // DOMOVETOTRASHFILESWORKER_H
