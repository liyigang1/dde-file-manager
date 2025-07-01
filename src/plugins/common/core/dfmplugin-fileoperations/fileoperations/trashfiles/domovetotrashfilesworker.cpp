// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "domovetotrashfilesworker.h"

#include <dfm-base/base/schemefactory.h>
#include <dfm-base/base/standardpaths.h>
#include <dfm-base/utils/universalutils.h>
#include <dfm-base/base/device/deviceutils.h>

#include <dfm-io/dfmio_utils.h>
#include <dfm-io/trashhelper.h>

#include <QUrl>
#include <QDebug>
#include <QtGlobal>
#include <QCryptographicHash>
#include <QStorageInfo>

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

USING_IO_NAMESPACE
DPFILEOPERATIONS_USE_NAMESPACE
DoMoveToTrashFilesWorker::DoMoveToTrashFilesWorker(QObject *parent)
    : FileOperateBaseWorker(parent)
{
    jobType = AbstractJobHandler::JobType::kMoveToTrashType;
    fstabMap = DeviceUtils::fstabBindInfo();
}

DoMoveToTrashFilesWorker::~DoMoveToTrashFilesWorker()
{
    stop();
}
/*!
 * \brief DoMoveToTrashFilesWorker::doWork the thread move to trash work function
 * \return move to trash is successed
 */
bool DoMoveToTrashFilesWorker::doWork()
{
    if (!AbstractWorker::doWork())
        return false;

    doMoveToTrash();

    endWork();

    return true;
}
/*!
 * \brief DoMoveToTrashFilesWorker::statisticsFilesSize init target arguments
 * \return
 */
bool DoMoveToTrashFilesWorker::statisticsFilesSize()
{
    sourceFilesCount = sourceUrls.size();
    targetUrl = FileUtils::trashRootUrl();
    return true;
}

void DoMoveToTrashFilesWorker::onUpdateProgress()
{
    emitProgressChangedNotify(completeFilesCount);
}
/*!
 * \brief DoMoveToTrashFilesWorker::doMoveToTrash do move to trash
 * \return move to trash success
 */
bool DoMoveToTrashFilesWorker::doMoveToTrash()
{
    bool result = false;
    DFMBASE_NAMESPACE::LocalFileHandler fileHandler;
    // 总大小使用源文件个数
    for (const auto &url : sourceUrls) {
        QUrl urlSource = url;
        if (!fstabMap.empty()) {
            for (const auto &device : fstabMap.keys()) {
                if (urlSource.path().startsWith(device)) {
                    urlSource.setPath(urlSource.path().replace(0, device.size(), fstabMap[device]));
                    break;
                }
            }
        }

        if (!stateCheck())
            return false;

        workData->currentOptCount.store(0);

        if (FileUtils::isTrashFile(urlSource)) {
            completeFilesCount++;
            completeSourceFiles.append(urlSource);
            continue;
        }

        // url是否可以删除 canrename
        if (!isCanMoveToTrash(urlSource, &result)) {
            if (result) {
                completeFilesCount++;
                completeSourceFiles.append(urlSource);
                continue;
            }
            return false;
        }

        const auto &fileInfo = InfoFactory::create<FileInfo>(urlSource, Global::CreateFileInfoType::kCreateFileInfoSync);
        if (!fileInfo) {
            // pause and emit error msg
            if (AbstractJobHandler::SupportAction::kSkipAction != doHandleErrorAndWait(urlSource, targetUrl, AbstractJobHandler::JobErrorType::kProrogramError)) {
                return false;
            } else {
                completeFilesCount++;
                continue;
            }
        }

        emitCurrentTaskNotify(urlSource, targetUrl);

        AbstractJobHandler::SupportAction action = AbstractJobHandler::SupportAction::kNoAction;
        do {
            action = AbstractJobHandler::SupportAction::kNoAction;
            QString trashTime = fileHandler.trashFile(urlSource);
            if (!trashTime.isEmpty()) {
                QUrl trashUrl = urlSource;
                trashUrl.setUserInfo(trashTime);

                completeTargetFiles.append(trashUrl);
                emitProgressChangedNotify(completeFilesCount);
                completeSourceFiles.append(urlSource);
                auto targetTash = trashTargetUrl(trashUrl);
                if (targetTash.isValid())
                    emit fileRenamed(urlSource, targetTash);
                continue;
            } else {
                // pause and emit error msg
                auto errmsg = QString("Unknown error");
                if (fileHandler.errorCode() == DFMIOErrorCode::DFM_IO_ERROR_NOT_SUPPORTED) {
                    errmsg = QString("The file can't be put into trash, you can use \"Shift+Del\" to delete the file completely.");
                } else if (fileHandler.errorCode() != DFMIOErrorCode::DFM_IO_ERROR_NONE) {
                    errmsg = fileHandler.errorString();
                }
                action = doHandleErrorAndWait(url, QUrl(),
                                              AbstractJobHandler::JobErrorType::kFileMoveToTrashError, false,
                                              fileHandler.errorCode() == DFMIOErrorCode::DFM_IO_ERROR_NONE ? "Unknown error"
                                                                                                           : fileHandler.errorString());
            }
        } while (action == AbstractJobHandler::SupportAction::kRetryAction && !isStopped());

        if (action == AbstractJobHandler::SupportAction::kDeleteAction) {
            deleteFiles(url, &fileHandler);
            completeFilesCount++;
            continue;
        }
        if (action == AbstractJobHandler::SupportAction::kNoAction
            || action == AbstractJobHandler::SupportAction::kSkipAction) {
            completeFilesCount++;
            continue;
        }

        return false;
    }
    return true;
}

/*!
 * \brief DoMoveToTrashFilesWorker::isCanMoveToTrash loop to check the source file can move to trash
 * \param url the source file url
 * \param result Output parameters, is skip this file
 * \return can move to trash
 */
bool DoMoveToTrashFilesWorker::isCanMoveToTrash(const QUrl &url, bool *result)
{
    AbstractJobHandler::SupportAction action = AbstractJobHandler::SupportAction::kNoAction;

    if (Q_UNLIKELY(!stateCheck())) {
        return false;
    }

    do {
        action = AbstractJobHandler::SupportAction::kNoAction;
        if (!canWriteFile(url))
            // pause and emit error msg
            action = doHandleErrorAndWait(url, targetUrl, AbstractJobHandler::JobErrorType::kPermissionError);

    } while (action == AbstractJobHandler::SupportAction::kRetryAction && !isStopped());

    if (action != AbstractJobHandler::SupportAction::kNoAction) {
        *result = action == AbstractJobHandler::SupportAction::kSkipAction;
        return false;
    }

    return true;
}

QUrl DoMoveToTrashFilesWorker::trashTargetUrl(const QUrl &url)
{
    auto fileUrl = url;
    if (!url.isValid() || url.scheme() != dfmbase::Global::Scheme::kFile)
        return QUrl();

    QMap<QUrl, QSharedPointer<TrashHelper::DeleteTimeInfo>> targetUrls;
    QList<QUrl> fileUrls;
    QStringList deleteInfo;
    auto userInfo = url.userInfo();
    deleteInfo = userInfo.split("-");
    // 错误处理
    if (deleteInfo.length() != 2)
        return QUrl();

    if (isStopped())
        return QUrl();

    QSharedPointer<TrashHelper::DeleteTimeInfo> info(new TrashHelper::DeleteTimeInfo);
    info->startTime = deleteInfo.first().toInt();
    info->endTime = deleteInfo.at(1).toInt();
    fileUrl.setUserInfo("");
    targetUrls.insert(fileUrl, info);

    QString errorMsg;
    TrashHelper trashHelper;
    trashHelper.setDeleteInfos(targetUrls);
    if (!trashHelper.getTrashUrls(&fileUrls, &errorMsg))
        return QUrl();
    if (fileUrls.length() <= 0)
        return QUrl();

    return fileUrls.first();
}

// 这个是file的插件，那么所有的文件都是file的scheme，都是本地文件，使用opendir加快文件迭代速度
void DoMoveToTrashFilesWorker::deleteFiles(const QUrl &url, LocalFileHandler *handler)
{
    if (!handler){
        qWarning() << " LocalFileHandler pointer is null , url = " << url;
        return;
    }

    struct stat64 statBuffer;
    if (::stat64(url.path().toStdString().data(), &statBuffer) != 0) {
        qWarning() << " stat64 url failed,url = " << url;
        return;
    }
    if (!FileUtils::symlinkTarget(url).isEmpty()) {
        handler->deleteFile(url);
        return;
    }

    QQueue<QUrl> urls;
    QStack<QUrl> deleteUrls;
    urls.enqueue(url);
    deleteUrls.push(url);

    while (!urls.isEmpty()) {
        if (Q_UNLIKELY(!stateCheck()))
            return;

        auto directoryUrl = urls.dequeue();
        DIR *dir { nullptr };
        struct dirent *entry { nullptr };

        if (!(dir = opendir(directoryUrl.path().toStdString().data()))) {
            qWarning() << "open dir failed, url = " << url << " , error : " << strerror(errno);
            continue;
        }

        FinallyUtil closeDir([=]{
            closedir(dir);
        });

        while ((entry = readdir(dir))) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            if (Q_UNLIKELY(!stateCheck()))
                return;

            struct stat64 statBufferCur;
            QString currentPath = directoryUrl.path() + QDir::separator() + entry->d_name;
            if (::stat64(currentPath.toStdString().data(), &statBufferCur) != 0) {
                qWarning() << " stat64 url failed , path = " << currentPath;
                continue;
            }

            QUrl currentFile = QUrl::fromLocalFile(currentPath);
            const auto &syslinkTag = FileUtils::symlinkTarget(currentFile);
            if (!syslinkTag.isEmpty() || !S_ISDIR(statBufferCur.st_mode)) {
                handler->deleteFile(currentFile);
                continue;
            }

            if (S_ISDIR(statBufferCur.st_mode)) {
                urls.enqueue(currentFile);
                deleteUrls.push(currentFile);
            }
        }
    }

    // 删除所有的目录
    while (!deleteUrls.isEmpty()) {
        if (Q_UNLIKELY(!stateCheck()))
            return;
        auto curDir = deleteUrls.pop();
        handler->deleteFile(curDir);
    }

}
