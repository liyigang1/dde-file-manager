// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dodeletefilesworker.h"
#include <dfm-base/base/schemefactory.h>
#include <dfm-base/utils/finallyutil.h>
#include <dfm-base/base/device/deviceutils.h>

#include <QUrl>
#include <QDebug>

#include <unistd.h>
#include <fts.h>

DPFILEOPERATIONS_USE_NAMESPACE
DoDeleteFilesWorker::DoDeleteFilesWorker(QObject *parent)
    : AbstractWorker(parent)
{
    jobType = AbstractJobHandler::JobType::kDeleteType;
}

DoDeleteFilesWorker::~DoDeleteFilesWorker()
{
    stop();
}

bool DoDeleteFilesWorker::doWork()
{
    if (!AbstractWorker::doWork())
        return false;

    deleteAllFiles();

    // 完成
    endWork();

    return true;
}

void DoDeleteFilesWorker::stop()
{
    // ToDo::停止删除的业务逻辑
    AbstractWorker::stop();
}

void DoDeleteFilesWorker::onUpdateProgress()
{
    emitProgressChangedNotify(deleteFilesCount);
}

/*!
 * \brief DoDeleteFilesWorker::deleteAllFiles delete All files
 * \return delete all files success
 */
bool DoDeleteFilesWorker::deleteAllFiles()
{
    // sources file list is checked
    // delete files on can't remove device
    useFts = FileOperationsUtils::useFtsDelete();
    if (useFts)
        return deleteFilesByFts();
    return deleteFilesOnCanNotRemoveDevice();
}

bool DoDeleteFilesWorker::deleteFilesByFts()
{
    if (sourceUrls.isEmpty())
        return false;

    QList<QByteArray> pathData;
    pathData.reserve(sourceUrls.size());
    for (const auto &url : sourceUrls) {
        if (!url.isLocalFile()) {
            fmWarning() << "deleteFilesByFts: skip non-local path:" << url;
            continue;
        }
        pathData.append(url.toLocalFile().toUtf8());
    }
    if (pathData.isEmpty())
        return false;

    QVector<char *> pathPtrs(pathData.size() + 1, nullptr);
    for (int i = 0; i < pathData.size(); ++i)
        pathPtrs[i] = pathData[i].data();

    FTS *fts = fts_open(pathPtrs.data(), FTS_PHYSICAL | FTS_NOSTAT | FTS_NOCHDIR, nullptr);
    if (!fts) {
        fmWarning() << "deleteFilesByFts: fts_open failed:" << strerror(errno);
        return false;
    }

    manualFileChangeNotifyNeeded = DeviceUtils::isSamba(sourceUrls.first()) || DeviceUtils::isFtp(sourceUrls.first());
    QSet<QUrl> sourceUrlsSet = sourceUrls.toSet();
    bool success = true;
    int errorCount = 0;

    FTSENT *ent;
    AbstractJobHandler::SupportAction action { AbstractJobHandler::SupportAction::kNoAction };
    while ((ent = fts_read(fts)) != nullptr) {
        if (!stateCheck()) {
            success = false;
            break;
        }

        bool isDir = false;
        bool shouldDelete = false;

        switch (ent->fts_info) {
        case FTS_F:
        case FTS_SL:
        case FTS_SLNONE:
        case FTS_NSOK:
            shouldDelete = true;
            isDir = false;
            break;
        case FTS_DP:
            shouldDelete = true;
            isDir = true;
            break;
        case FTS_DNR:
        case FTS_ERR:
        case FTS_DC:
            fmWarning() << "deleteFilesByFts: traversal error:" << ent->fts_path
                       << strerror(ent->fts_errno);
            errorCount++;
            continue;
        default:
            continue;
        }

        if (!shouldDelete)
            continue;

        auto url = QUrl::fromLocalFile(ent->fts_path);
        emitCurrentTaskNotify(url, QUrl());
        do {
            action = AbstractJobHandler::SupportAction::kNoAction;
            int ret = isDir ? ::rmdir(ent->fts_accpath) != 0 : ::unlink(ent->fts_accpath);
            if (ret != 0) {
                if (errno == ENOENT || errno == ENOTDIR)
                    continue;
                fmWarning() << "deleteFilesByFts: delete failed:" << ent->fts_path
                           << strerror(errno);
                errorCount++;
                action = doHandleErrorAndWait(url, AbstractJobHandler::JobErrorType::kDeleteFileError,
                                              strerror(errno));
            }
        } while (!isStopped() && action == AbstractJobHandler::SupportAction::kRetryAction);

        if (action == AbstractJobHandler::SupportAction::kSkipAction)
            continue;   // 跳过，不发送信号

        batchEmitFileDeleted(url);

        if (manualFileChangeNotifyNeeded)
            FileUtils::notifyFileChangeManual(DFMGLOBAL_NAMESPACE::FileNotifyType::kFileDeleted, url);

        if (sourceUrlsSet.contains(url)) {
            completeSourceFiles.append(url);
            completeTargetFiles.append(url);
        }


        deleteFilesCount++;
    }

    fts_close(fts);

    flushFileDeletedBatch();

    // 这里使用fmWarning，专门这么处理，以免用户数据丢失没有日志
    fmWarning() << "deleteFilesByFts: done —" << errorCount << "errors," << deleteFilesCount << "deleted";

    return success && errorCount == 0;
}

void DoDeleteFilesWorker::flushFileDeletedBatch()
{
    if (!fileDeletedBuffer.isEmpty()) {
        emit fileDeleted(fileDeletedBuffer);
        fileDeletedBuffer.clear();
    }
}

void DoDeleteFilesWorker::batchEmitFileDeleted(const QUrl &url)
{
    if (fileDeletedBuffer.isEmpty())
        fileDeletedTimer.start();

    fileDeletedBuffer.append(url);

    if (fileDeletedBuffer.size() >= 1000 || fileDeletedTimer.hasExpired(500))
        flushFileDeletedBatch();
}

/*!
 * \brief DoDeleteFilesWorker::deleteFilesOnCanNotRemoveDevice Delete files on non removable devices
 * \return delete file success
 */
bool DoDeleteFilesWorker::deleteFilesOnCanNotRemoveDevice()
{
    if (allFilesList.count() == 1 && isConvert) {
        auto info = InfoFactory::create<FileInfo>(allFilesList.first(), Global::CreateFileInfoType::kCreateFileInfoSync);
        if (info)
            deleteFirstFileSize = info->size();
    }

    AbstractJobHandler::SupportAction action { AbstractJobHandler::SupportAction::kNoAction };
    for (QList<QUrl>::iterator it = --allFilesList.end(); it != --allFilesList.begin(); --it) {
        if (!stateCheck())
            return false;
        workData->currentOptCount.store(0);
        const QUrl &url = *it;
        emitCurrentTaskNotify(url, QUrl());
        do {
            action = AbstractJobHandler::SupportAction::kNoAction;
            if (!localFileHandler->deleteFile(url)) {
                action = doHandleErrorAndWait(url, AbstractJobHandler::JobErrorType::kDeleteFileError,
                                              localFileHandler->errorString());
            } else if (manualFileChangeNotifyNeeded) {
                FileUtils::notifyFileChangeManual(DFMGLOBAL_NAMESPACE::FileNotifyType::kFileDeleted, url);
            }
        } while (!isStopped() && action == AbstractJobHandler::SupportAction::kRetryAction);

        if (sourceUrls.contains(url)) {
            if (action == AbstractJobHandler::SupportAction::kNoAction) {
                completeSourceFiles.append(url);
                completeTargetFiles.append(url);
            }
        }

        deleteFilesCount++;

        if (action == AbstractJobHandler::SupportAction::kSkipAction)
            continue;

        if (action != AbstractJobHandler::SupportAction::kNoAction) {
            flushFileDeletedBatch();
            return false;
        }

        batchEmitFileDeleted(url);
    }
    flushFileDeletedBatch();
    fmWarning() << "localFileHandler->deleteFile: done — " << deleteFilesCount << "deleted";
    return true;
}
/*!
 * \brief DoDeleteFilesWorker::deleteFilesOnOtherDevice Delete files on removable devices and other
 * \return delete file success
 */
bool DoDeleteFilesWorker::deleteFilesOnOtherDevice()
{
    bool ok = true;
    if (sourceUrls.count() == 1 && isConvert) {
        auto info = InfoFactory::create<FileInfo>(sourceUrls.first(), Global::CreateFileInfoType::kCreateFileInfoSync);
        if (info)
            deleteFirstFileSize = info->size();
    }
    for (auto &url : sourceUrls) {
        const auto &info = InfoFactory::create<FileInfo>(url, Global::CreateFileInfoType::kCreateFileInfoSync);
        if (!info) {
            // pause and emit error msg
            if (doHandleErrorAndWait(url, AbstractJobHandler::JobErrorType::kProrogramError) == AbstractJobHandler::SupportAction::kSkipAction)
                continue;
            return false;
        }

        if (info->isAttributes(OptInfoType::kIsSymLink) || info->isAttributes(OptInfoType::kIsFile)) {
            ok = deleteFileOnOtherDevice(url);
        } else {
            ok = deleteDirOnOtherDevice(info);
        }

        if (!ok)
            return false;
        completeTargetFiles.append(url);
        completeSourceFiles.append(url);
    }
    return true;
}
/*!
 * \brief DoDeleteFilesWorker::deleteFileOnOtherDevice Delete file on removable devices and other
 * \param url delete url
 * \return delete success
 */
bool DoDeleteFilesWorker::deleteFileOnOtherDevice(const QUrl &url)
{
    if (!stateCheck())
        return false;

    emitCurrentTaskNotify(url, QUrl());

    AbstractJobHandler::SupportAction action { AbstractJobHandler::SupportAction::kNoAction };
    do {
        action = AbstractJobHandler::SupportAction::kNoAction;
        if (!localFileHandler->deleteFile(url)) {
            action = doHandleErrorAndWait(url, AbstractJobHandler::JobErrorType::kDeleteFileError,
                                          localFileHandler->errorString());
        } else if (manualFileChangeNotifyNeeded) {
            FileUtils::notifyFileChangeManual(DFMGLOBAL_NAMESPACE::FileNotifyType::kFileDeleted, url);
        }
    } while (!isStopped() && action == AbstractJobHandler::SupportAction::kRetryAction);

    deleteFilesCount++;

    if (action == AbstractJobHandler::SupportAction::kSkipAction)
        return true;

    return action == AbstractJobHandler::SupportAction::kNoAction;
}
/*!
 * \brief DoDeleteFilesWorker::deleteDirOnOtherDevice Delete dir on removable devices and other
 * \param dir delete dir
 * \return delete success
 */
bool DoDeleteFilesWorker::deleteDirOnOtherDevice(const FileInfoPointer &dir)
{
    if (!stateCheck())
        return false;

    if (dir->countChildFile() < 0)
        return deleteFileOnOtherDevice(dir->urlOf(UrlInfoType::kUrl));

    AbstractJobHandler::SupportAction action { AbstractJobHandler::SupportAction::kNoAction };
    AbstractDirIteratorPointer iterator(nullptr);
    do {
        action = AbstractJobHandler::SupportAction::kNoAction;
        QString errorMsg;
        iterator = DirIteratorFactory::create<AbstractDirIterator>(dir->urlOf(UrlInfoType::kUrl), &errorMsg);
        if (!iterator) {
            action = doHandleErrorAndWait(dir->urlOf(UrlInfoType::kUrl), AbstractJobHandler::JobErrorType::kDeleteFileError, errorMsg);
        }
    } while (!isStopped() && action == AbstractJobHandler::SupportAction::kRetryAction);

    if (action == AbstractJobHandler::SupportAction::kSkipAction)
        return true;
    if (action != AbstractJobHandler::SupportAction::kNoAction)
        return false;

    bool ok { true };
    FinallyUtil closeIterator([iterator]{
        iterator->close();
    });
    while (iterator->hasNext()) {
        const QUrl &url = iterator->next();

        const auto &info = InfoFactory::create<FileInfo>(url, Global::CreateFileInfoType::kCreateFileInfoSync);;
        if (!info) {
            // pause and emit error msg
            if (doHandleErrorAndWait(url, AbstractJobHandler::JobErrorType::kProrogramError) == AbstractJobHandler::SupportAction::kSkipAction)
                continue;
            return false;
        }

        if (info->isAttributes(OptInfoType::kIsSymLink) || info->isAttributes(OptInfoType::kIsFile)) {
            ok = deleteFileOnOtherDevice(url);
        } else {
            ok = deleteDirOnOtherDevice(info);
        }

        if (!ok)
            return false;
    }

    // delete self dir
    return deleteFileOnOtherDevice(dir->urlOf(UrlInfoType::kUrl));
}
/*!
 * \brief DoCopyFilesWorker::doHandleErrorAndWait Blocking handles errors and returns
 * actions supported by the operation
 * \param from source information
 * \param to target information
 * \param error error type
 * \param needRetry is neef retry action
 * \param errorMsg error message
 * \return support action
 */
AbstractJobHandler::SupportAction
DoDeleteFilesWorker::doHandleErrorAndWait(const QUrl &from,
                                          const AbstractJobHandler::JobErrorType &error,
                                          const QString &errorMsg)
{
    if (workData->errorOfAction.contains(error) && workData->currentOptCount < 4) {
        currentAction = workData->errorOfAction.value(error);
        if (currentAction == AbstractJobHandler::SupportAction::kRetryAction) {
            workData->currentOptCount++;
            QThread::msleep(100);
        }
        return currentAction;
    }

    workData->currentOptCount.store(0);

    setStat(AbstractJobHandler::JobState::kPauseState);
    emitErrorNotify(from, QUrl(), error, false, 0, errorMsg);

    {
        QMutexLocker locker(&mutex);
        waitCondition.wait(&mutex);
    }

    return currentAction;
}
