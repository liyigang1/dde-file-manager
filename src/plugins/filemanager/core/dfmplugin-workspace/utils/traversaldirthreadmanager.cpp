// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "traversaldirthreadmanager.h"
#include "events/workspaceeventsequence.h"

#include <dfm-base/utils/keywordextractor.h>
#include <dfm-base/base/schemefactory.h>
#include <dfm-base/file/local/localdiriterator.h>
#include <dfm-base/utils/fileutils.h>
#include <dfm-base/utils/networkutils.h>
#include <dfm-base/utils/finallyutil.h>

#include <QElapsedTimer>
#include <QDebug>

#include <sys/stat.h>

typedef QList<QSharedPointer<DFMBASE_NAMESPACE::SortFileInfo>>& SortInfoList;

using namespace dfmbase;
using namespace dfmplugin_workspace;
USING_IO_NAMESPACE

TraversalDirThreadManager::TraversalDirThreadManager(const QUrl &url,
                                                     const QStringList &nameFilters,
                                                     QDir::Filters filters,
                                                     QDirIterator::IteratorFlags flags,
                                                     QObject *parent)
    : TraversalDirThread(url, nameFilters, filters, flags, parent)
{
    qRegisterMetaType<QList<FileInfoPointer>>();
    qRegisterMetaType<FileInfoPointer>();
    qRegisterMetaType<QList<SortInfoPointer>>();
    qRegisterMetaType<SortInfoPointer>();
    traversalToken = QString::number(quintptr(this), 16);
}

TraversalDirThreadManager::~TraversalDirThreadManager()
{
    quit();
    wait();
    if (future) {
        future->deleteLater();
        future = nullptr;
    }
}

void TraversalDirThreadManager::setSortAgruments(const Qt::SortOrder order, const Global::ItemRoles sortRole, const bool isMixDirAndFile)
{
    sortOrder = order;
    this->isMixDirAndFile = isMixDirAndFile;
    switch (sortRole) {
    case Global::ItemRoles::kItemFileDisplayNameRole:
        this->sortRole = dfmio::DEnumerator::SortRoleCompareFlag::kSortRoleCompareFileName;
        break;
    case Global::ItemRoles::kItemFileSizeRole:
        this->sortRole = dfmio::DEnumerator::SortRoleCompareFlag::kSortRoleCompareFileSize;
        break;
    case Global::ItemRoles::kItemFileLastReadRole:
        this->sortRole = dfmio::DEnumerator::SortRoleCompareFlag::kSortRoleCompareFileLastRead;
        break;
    case Global::ItemRoles::kItemFileLastModifiedRole:
        this->sortRole = dfmio::DEnumerator::SortRoleCompareFlag::kSortRoleCompareFileLastModified;
        break;
    default:
        this->sortRole = dfmio::DEnumerator::SortRoleCompareFlag::kSortRoleCompareDefault;
    }
}

void TraversalDirThreadManager::setTraversalToken(const QString &token)
{
    traversalToken = token;
}

void TraversalDirThreadManager::start()
{
    running = true;
    if (this->sortRole != dfmio::DEnumerator::SortRoleCompareFlag::kSortRoleCompareDefault
            && dirIterator->oneByOne())
        dirIterator->setProperty("QueryAttributes","standard::name,standard::type,standard::is-file,standard::is-dir,"
                                                   "standard::size,standard::is-symlink,standard::symlink-target,access::*,time::*");
    auto local = dirIterator.dynamicCast<LocalDirIterator>();
    if (local && local->oneByOne()) {
        future = local->asyncIterator();
        if (future) {
            connect(future, &DEnumeratorFuture::asyncIteratorOver, this, &TraversalDirThreadManager::onAsyncIteratorOver);
            future->startAsyncIterator();
            return;
        }
    }

    if (stopFlag.load(std::memory_order_acquire)) {
        running = false;
        return;
    }

    TraversalDirThread::start();
}

bool TraversalDirThreadManager::isRunning() const
{
    return running;
}

void TraversalDirThreadManager::onAsyncIteratorOver()
{
    Q_EMIT iteratorInitFinished();
    TraversalDirThread::start();
}

void TraversalDirThreadManager::run()
{
    FinallyUtil setRun([this]{
        running = false;
    });

    if (dirIterator.isNull()) {
        emit traversalFinished(traversalToken);
        return;
    }

    QElapsedTimer timer;
    timer.start();
    fmInfo() << "dir query start, url: " << dirUrl;

    if (stopFlag.load(std::memory_order_acquire))
        return;

    int count = 0;
    if (!dirIterator->oneByOne()) {
        const QList<SortInfoPointer> &fileList = iteratorAll();
        count = fileList.count();
        fmInfo() << "local dir query end, file count: " << count << " url: " << dirUrl << " elapsed: " << timer.elapsed();
    } else {
        count = iteratorOneByOne(timer);
        fmInfo() << "dir query end, file count: " << count << " url: " << dirUrl << " elapsed: " << timer.elapsed();
    }
}

int TraversalDirThreadManager::iteratorOneByOne(const QElapsedTimer &timere)
{
    dirIterator->cacheBlockIOAttribute();
    fmInfo() << "cacheBlockIOAttribute finished, url: " << dirUrl << " elapsed: " << timere.elapsed();
    if (stopFlag.load(std::memory_order_acquire)) {
        emit traversalFinished(traversalToken);
        return 0;
    }

    if (!dirIterator->initIterator()) {
        fmWarning() << "dir iterator init failed !! url : " << dirUrl;
        emit traversalFinished(traversalToken);
        return 0;
    }

    if (!future)
        Q_EMIT iteratorInitFinished();

    if (!timer)
        timer = new QElapsedTimer();

    timer->restart();

    QList<FileInfoPointer> childrenList;   // 当前遍历出来的所有文件
    QSet<QUrl> urls;
    int filecount = 0;
    while (dirIterator->hasNext()) {
        if (stopFlag.load(std::memory_order_acquire))
            break;

        // 调用一次fileinfo进行文件缓存
        const auto &fileUrl = dirIterator->next();
        if (!fileUrl.isValid())
            continue;
        if (urls.contains(fileUrl))
            continue;
        urls.insert(fileUrl);
        auto fileInfo = dirIterator->fileInfo();
        if (fileUrl.isValid() && !fileInfo) {
            fileInfo = InfoFactory::create<FileInfo>(fileUrl);
        } else if (!fileInfo.isNull()) {
            InfoFactory::cacheFileInfo(fileInfo);
        }

        if (!fileInfo)
            continue;

        childrenList.append(fileInfo);
        filecount++;

        if (timer->elapsed() > timeCeiling || childrenList.count() > countCeiling) {
            emit updateChildrenManager(childrenList, traversalToken);
            timer->restart();
            childrenList.clear();
        }
    }

    if (childrenList.length() > 0)
        emit updateChildrenManager(childrenList, traversalToken);

    emit traversalRequestSort(traversalToken);

    emit traversalFinished(traversalToken);

    QVariant findErrorFile = dirIterator->property("hasErrorFile");
    if (findErrorFile.isValid() && findErrorFile.toBool() == true) {
         emit traversalFindErrorFile(traversalToken);
    }

    return filecount;
}

QList<SortInfoPointer> TraversalDirThreadManager::iteratorAll()
{
    fmDebug() << "Starting batch mode iteration for URL:" << dirUrl.toString();

    fmDebug() << "Iterator arguments set - sortRole:" << static_cast<int>(sortRole)
              << "mixFileAndDir:" << isMixDirAndFile << "sortOrder:" << sortOrder;
    if (stopFlag.load(std::memory_order_acquire))
        return {};

    Q_EMIT iteratorInitFinished();

    if (stopFlag.load(std::memory_order_acquire))
        return {};

    // Get the initial list of files
    auto fileList = dirIterator->sortFileInfoList();

    fmInfo() << "Initial file list retrieved - count:" << fileList.size() << "token:" << traversalToken;

    if (stopFlag.load(std::memory_order_acquire))
        return {};

    // Emit the initial file list
    emit updateLocalChildren(fileList, sortRole, sortOrder, isMixDirAndFile, traversalToken);

    // Check if the iterator is waiting for more updates (search still in progress, etc.)
    while (dirIterator->isWaitingForUpdates()) {
        if (stopFlag.load(std::memory_order_acquire))
            return {};

        fileList = dirIterator->sortFileInfoList();
        if (!fileList.isEmpty())
            emit updateChildrenInfo(fileList, traversalToken);
    }

    emit traversalRequestSort(traversalToken);

    // Iterator is not waiting for updates, so signal that we're done
    emit traversalFinished(traversalToken);

    return fileList;
}
