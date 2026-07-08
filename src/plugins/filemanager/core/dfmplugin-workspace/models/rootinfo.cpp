// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "rootinfo.h"
#include "fileitemdata.h"

#include <dfm-base/base/schemefactory.h>
#include <dfm-base/utils/universalutils.h>
#include <dfm-base/base/application/settings.h>
#include <dfm-base/utils/fileutils.h>
#include <dfm-base/utils/keywordextractor.h>
#include <dfm-base/utils/sortfileinfoutils.h>

#include <dfm-framework/event/event.h>

#include <dfm-io/dfmio_utils.h>
#include <dfm-io/dfile.h>

#include <QApplication>
#include <QtConcurrent>
#include <QElapsedTimer>

using namespace dfmbase;
using namespace dfmplugin_workspace;


FileWatcherWorker::FileWatcherWorker(RootInfoWorker *root, QObject *parent)
    : QObject(parent), rootptr(root)
{
}

FileWatcherWorker::~FileWatcherWorker()
{

}

void FileWatcherWorker::doFileDeleted(const QUrl &fileUrl)
{
    assert(qApp->thread() != QThread::currentThread());
    fmInfo() << "FileWatcherWorker::doFileDeleted: url=" << fileUrl << "stopped=" << rootptr->stopped;
    // 自己删除可以执行
    if (rootptr->stopped || !isSubFile(fileUrl))
        return;

    FinallyUtil time([this]{
        doCheckAndStartTimer();
    });
    // 删除的是自己
    if (UniversalUtils::urlEquals(fileUrl, rootptr->url)) {
        fmWarning() << "Root directory deleted:" << fileUrl;
        // 移除缓存和监视器
        emit InfoCacheController::instance().removeCacheFileInfo({ fileUrl });
        WatcherCache::instance().removeCacheWatcherByParent(fileUrl);

        // 处理和关闭窗口
        emit rootptr->requestCloseTab(fileUrl);
        emit rootptr->requestClearRoot(fileUrl);
        rootptr->childrenUrlList.clear();
        rootptr->sourceDataList.clear();
        return;
    }

    adds.remove(fileUrl);
    updates.remove(fileUrl);
    if (!removes.contains(fileUrl))
        removes.insert(fileUrl);

}

void FileWatcherWorker::doFileMoved(const QUrl &fromUrl, const QUrl &toUrl)
{
    assert(qApp->thread() != QThread::currentThread());
    fmInfo() << "FileWatcherWorker::dofileMoved: from=" << fromUrl << "to=" << toUrl;
    if (rootptr->stopped)
        return;
    // 补充：发射重命名过程开始信号（UI反馈）
    emit rootptr->renameFileProcessStarted();
    doFileDeleted(fromUrl);
    // 补充：刷新目标URL缓存
    auto info = InfoCacheController::instance().getCacheInfo(toUrl);
    if (info)
        info->updateAttributes();

    doFileCreated(toUrl);
}

void FileWatcherWorker::doFileCreated(const QUrl &fileUrl)
{
    assert(qApp->thread() != QThread::currentThread());
    fmInfo() << "FileWatcherWorker::dofileCreated: url=" << fileUrl << "stopped=" << rootptr->stopped;
    if (rootptr->stopped || !isSubFile(fileUrl))
        return;

    // 判断update和remove中是否存在存在就移除
    updates.remove(fileUrl);
    removes.remove(fileUrl);
    if (!adds.contains(fileUrl))
        adds.insert(fileUrl);

    doCheckAndStartTimer();
}

void FileWatcherWorker::doFileUpdated(const QUrl &fileUrl)
{
    assert(qApp->thread() != QThread::currentThread());
    fmInfo() << "FileWatcherWorker::doFileUpdated: url=" << fileUrl << "stopped=" << rootptr->stopped;
    if (rootptr->stopped || !isSubFile(fileUrl) || UniversalUtils::urlEquals(fileUrl, rootptr->url))
        return;

    FinallyUtil time([this]{
        doCheckAndStartTimer();
    });
    // 如果在增加或者移除中就不添加
    if (adds.contains(fileUrl) || removes.contains(fileUrl) || updates.contains(fileUrl))
        return;

    bool fileShowNow  = rootptr->childrenUrlList.contains(fileUrl);
    // 收到update信号但是没有收到fileadd信号，那么添加一个fileadd信号
    if (fileShowNow) {
        updates.insert(fileUrl);
    } else if (dfmio::DFile(fileUrl).exists()) {
        adds.insert(fileUrl);
    }
}

void FileWatcherWorker::doWatcherEvent()
{
    assert(qApp->thread() != QThread::currentThread());
    delayTimeStart = false;
    doWatcherSubEvent();
}

void FileWatcherWorker::doWatcherSubEvent()
{
    if (rootptr->stopped)
        return;
    // 处理添加文件
    if (!removes.isEmpty()) {
        rootptr->removeChildren(removes);
        removes.clear();
    }
    if (!adds.isEmpty()) {
        rootptr->addChildren(adds);
        adds.clear();
    }
    if (!updates.isEmpty()) {
        rootptr->updateChildren(updates);
        updates.clear();
    }
}

void FileWatcherWorker::doCheckAndStartTimer()
{
    if (delayTimeStart)
        return;
    delayTimeStart = true;
    emit rootptr->watcherTimerStart();
}

bool FileWatcherWorker::isSubFile(const QUrl &fileUrl)
{
    // 不是本地文件判读不了是否是子目录
    if (!fileUrl.isLocalFile())
        return true;

    auto url = fileUrl;
    // 判断是否是当前目录的子文件，不是就不处理
    auto parentUrl = rootptr->url;
    // 清除掉用户信息和查询信息
    parentUrl.setUserInfo(QString());
    parentUrl.setQuery(QString());
    url.setUserInfo(QString());
    url.setQuery(QString());
    // 自己删除可以执行
    if (!url.toString().startsWith(parentUrl.toString()))
        return false;
    return true;
}

FileIteratorWorker::FileIteratorWorker(RootInfoWorker *root, QObject *parent)
    : QObject(parent), rootptr(root)
{

}

FileIteratorWorker::~FileIteratorWorker()
{

}

// 处理迭代器使用next迭代出来的文件
void FileIteratorWorker::handleTraversalResults(const QList<FileInfoPointer> &children, const QString &travseToken)
{
    assert(qApp->thread() != QThread::currentThread());
    fmInfo() << "FileIteratorWorker::handleTraversalResults: children count=" << children.size() << "token=" << travseToken << "stopped=" << rootptr->stopped;
    if (rootptr->stopped)
        return;

    QList<SortInfoPointer> sortInfos;
    QList<FileInfoPointer> infos;
    std::for_each(children.begin(), children.end(), [this, &sortInfos, &infos](const FileInfoPointer &info){
        if (rootptr->stopped)
            return;
        auto sortInfo = rootptr->addChild(info);
        if (!sortInfo)
            return ;
        sortInfos.append(sortInfo);

        infos.append(info);
    });

    fmDebug() << "FileIteratorWorker::handleTraversalResults: sortInfos count=" << sortInfos.size();
    if (sortInfos.length() > 0)
        Q_EMIT rootptr->iteratorAddFiles(travseToken, sortInfos, infos);
}

// 处理search的后面的搜索结果
void FileIteratorWorker::handleTraversalResultsUpdate(const QList<SortInfoPointer> &children, const QString &travseToken, const bool increment)
{
    assert(qApp->thread() != QThread::currentThread());
    if (children.isEmpty() || rootptr->stopped)
        return;

    rootptr->addChildren(children, increment);

    emit rootptr->iteratorUpdateFiles(travseToken, rootptr->sourceDataList, false);
}

// 处理搜索的第一次推送的结果或者迭代器使用sortFileInfoList迭代出来的文件
void FileIteratorWorker::handleTraversalLocalResult(QList<SortInfoPointer> children, dfmio::DEnumerator::SortRoleCompareFlag sortRole, Qt::SortOrder sortOrder, bool isMixDirAndFile, const QString &travseToken)
{
    assert(qApp->thread() != QThread::currentThread());
    if (rootptr->stopped)
        return;
    rootptr->originSortRole = sortRole;
    rootptr->originSortOrder = sortOrder;
    rootptr->originMixSort = isMixDirAndFile;

    rootptr->addChildren(children, false);

    Q_EMIT rootptr->iteratorLocalFiles(travseToken,rootptr->sourceDataList, rootptr->originSortRole, rootptr->originSortOrder, rootptr->originMixSort, true);
}

void FileIteratorWorker::handleTraversalFinish(const QString &travseToken)
{
    if (rootptr->stopped)
        return;
    rootptr->itStatus = RootInfoWorker::IteratorStatus::kFinished;
    emit rootptr->traversalFinished(travseToken);
}

void FileIteratorWorker::handleTraversalSort(const QString &travseToken)
{
    if (rootptr->stopped)
        return;
    emit rootptr->requestSort(travseToken, rootptr->url);
}

void FileIteratorWorker::handleGetSourceData(const QString &currentToken)
{
    if (rootptr->stopped)
        return;

    QList<SortInfoPointer> newDatas = rootptr->sourceDataList;

    emit rootptr->sourceDatas(currentToken, newDatas, rootptr->originSortRole, rootptr->originSortOrder, rootptr->originMixSort, rootptr->itStatus == RootInfoWorker::IteratorStatus::kFinished);
    if (rootptr->itStatus == RootInfoWorker::IteratorStatus::kFinished) {
        emit rootptr->requestSort(currentToken, rootptr->url);
        emit rootptr->traversalFinished(currentToken);
    }
}

RootInfoWorker::RootInfoWorker(const QUrl &url, QObject *parent)
    : QObject (parent), url(url)
{
    hiddenFileUrl.setScheme(url.scheme());
    hiddenFileUrl.setPath(DFMIO::DFMUtils::buildFilePath(url.path().toStdString().c_str(), ".hidden", nullptr));
    it.reset(new FileIteratorWorker(this));
    watch.reset(new FileWatcherWorker(this));
    keyWords = KeywordExtractorManager::instance().extractor().extractFromUrl(url);
}

RootInfoWorker::~RootInfoWorker()
{

}

void RootInfoWorker::stop()
{
    stopped = true;
}

QSharedPointer<FileIteratorWorker> RootInfoWorker::iteratorWorker() const
{
    return it;
}

QSharedPointer<FileWatcherWorker> RootInfoWorker::watcherWorker() const
{
    return watch;
}

void RootInfoWorker::addChildren(const QSet<QUrl> &urlList)
{
    if (stopped)
        return;
    fmInfo() << "RootInfoWorker::addChildren(QSet): url count=" << urlList.size() << "stopped=" << stopped;
    QList<SortInfoPointer> newSortInfo;

    bool isContainHidd = false;
    auto hideFiles = SortFileInfoUtils::loadHideFileList(url);
    std::for_each(urlList.begin(), urlList.end(),[this, &isContainHidd, &newSortInfo, hideFiles](QUrl url) {
        if (stopped)
            return;
        url.setPath(url.path());

        if (UniversalUtils::urlEquals(url, hiddenFileUrl))
            isContainHidd = true;

        SortInfoPointer sortInfo{nullptr};

        if (url.isLocalFile()) {
            sortInfo = addChild(url, hideFiles);
        } else {
            auto child = fileInfo(url);

            if (!child)
                return;

            sortInfo = addChild(child);
        }

        if (sortInfo)
            newSortInfo.append(sortInfo);
    });

    fmDebug() << "RootInfoWorker::addChildren(QSet): newSortInfo count=" << newSortInfo.size();
    if (newSortInfo.count() > 0) {
        originSortRole = dfmio::DEnumerator::SortRoleCompareFlag::kSortRoleCompareDefault;
        emit watcherAddFiles(newSortInfo);
    }

    if (isContainHidd)
        Q_EMIT watcherUpdateHideFile(hiddenFileUrl);
}

void RootInfoWorker::addChildren(const QList<SortInfoPointer> &children, const bool increment)
{
    if (stopped)
        return;

    if (!increment) {
        // 这里搜索时不能使用增量更新，以前的SortInfo也有修改，所以前面搜索出来的有可能搜索的内容没有显示出来
        childrenUrlList.clear();
        sourceDataList = children;
    }

    std::for_each(children.begin(), children.end(), [this, increment](const SortInfoPointer file){
        if (!file || stopped)
            return;
        childrenUrlList.append(file->fileUrl());
        if (increment)
            sourceDataList.append(file);
    });
}

SortInfoPointer RootInfoWorker::addChild(const FileInfoPointer &child)
{
    if (!child)
        return nullptr;

    QUrl childUrl = child->urlOf(dfmbase::UrlInfoType::kUrl);
    childUrl.setPath(childUrl.path());

    SortInfoPointer sort = sortFileInfo(child);
    if (!sort)
        return nullptr;

    if (childrenUrlList.contains(childUrl)) {
        sourceDataList.replace(childrenUrlList.indexOf(childUrl), sort);
        return sort;
    }

    childrenUrlList.append(childUrl);
    sourceDataList.append(sort);

    return sort;
}

SortInfoPointer RootInfoWorker::sortFileInfo(const FileInfoPointer &info)
{
    if (!info || stopped)
        return nullptr;
    SortInfoPointer sortInfo(new SortFileInfo);
    sortInfo->setUrl(info->urlOf(UrlInfoType::kUrl));
    sortInfo->setSize(info->size());
    sortInfo->setFile(!info->isAttributes(OptInfoType::kIsDir));
    sortInfo->setDir(info->isAttributes(OptInfoType::kIsDir));
    sortInfo->setHide(info->isAttributes(OptInfoType::kIsHidden));
    sortInfo->setSymlink(info->isAttributes(OptInfoType::kIsSymLink));
    sortInfo->setReadable(info->isAttributes(OptInfoType::kIsReadable));
    sortInfo->setWriteable(info->isAttributes(OptInfoType::kIsWritable));
    sortInfo->setExecutable(info->isAttributes(OptInfoType::kIsExecutable));
    SortFileInfoUtils::cacheLastReadTime(sortInfo, info);
    return sortInfo;
}

void RootInfoWorker::removeChildren(const QSet<QUrl> &urlList)
{
    if (stopped)
        return;
    QList<SortInfoPointer> removeChildren {};
    int childIndex = -1;
    QList<QUrl> removeUrls;
    emit InfoCacheController::instance().removeCacheFileInfo(urlList.toList());
    QSet<QString> hideFiles;
    if (url.isLocalFile())
        hideFiles = SortFileInfoUtils::loadHideFileList(url);
    std::for_each(urlList.begin(), urlList.end(), [this, &removeUrls, &removeChildren, &childIndex, hideFiles](const QUrl &fileUrl){
        if (stopped)
            return;
        auto url = fileUrl;
        url.setPath(url.path());
        SortInfoPointer sortInfo{nullptr};
        auto realUrl = url;
        if (url.isLocalFile()) {
            sortInfo = SortFileInfoUtils::createSortInfo(url, hideFiles);
        } else {
            auto child = fileInfo(url);
            if (!child)
                return;

            realUrl = child->urlOf(UrlInfoType::kUrl);
            sortInfo = sortFileInfo(child);
        }

        if (sortInfo.isNull()) {
            sortInfo.reset(new SortFileInfo);
            sortInfo->setUrl(realUrl);
        }
        removeUrls.append(realUrl);
        childIndex = childrenUrlList.indexOf(realUrl);
        if (childIndex < 0 || childIndex >= childrenUrlList.length()) {
            removeChildren.append(sortInfo);
            return;
        }
        auto oldSort = sourceDataList.takeAt(childIndex);
        // 处理各个窗口显示的父目录删除
        if (sortInfo && sortInfo->isDir()) {
            WatcherCache::instance().removeCacheWatcherByParent(url);
            emit requestCloseTab(url);
        }

        childrenUrlList.removeAt(childIndex);
        removeChildren.append(oldSort);
    });

    // 移除缓存
    if (removeUrls.count() > 0)
        emit InfoCacheController::instance().removeCacheFileInfo(removeUrls);

    // 移除显示的项
    if (removeChildren.count() > 0)
        emit watcherRemoveFiles(removeChildren);

    // 跟新隐藏文件的显示项
    if (removeUrls.contains(hiddenFileUrl))
        Q_EMIT watcherUpdateHideFile(hiddenFileUrl);
}

FileInfoPointer RootInfoWorker::fileInfo(const QUrl &url)
{
    if (stopped)
        return nullptr;
    FileInfoPointer info = InfoFactory::create<FileInfo>(url, Global::CreateFileInfoType::kCreateFileInfoSync);
    if (!info.isNull())
        return info;

    // 不是当前目录的就不处理
    const QUrl &parentUrl = QUrl::fromPercentEncoding(this->url.toString().toUtf8());
    auto path = url.path();
    if (path.isEmpty() || path == QDir::separator() || url.fileName().isEmpty())
        return info;

    auto pathParent = path.endsWith(QDir::separator()) ? path.left(path.length() - 1) : path;
    auto parentPath = parentUrl.path().endsWith(QDir::separator())
            ? parentUrl.path().left(parentUrl.path().length() - 1)
            : parentUrl.path();
    pathParent = pathParent.left(pathParent.lastIndexOf(QDir::separator()));
    if (!parentPath.endsWith(pathParent.mid(1)))
        return info;

    auto currentUrl = parentUrl;
    currentUrl.setPath(currentUrl.path(QUrl::PrettyDecoded) + QDir::separator() + url.fileName());
    info = InfoFactory::create<FileInfo>(currentUrl);
    return info;
}

SortInfoPointer RootInfoWorker::updateChild(const QUrl &url, const QSet<QString> &hideFiles)
{
    if (stopped)
        return nullptr;
    SortInfoPointer sort { nullptr };
    auto realUrl = url;
    if (url.isLocalFile()) {
        sort = SortFileInfoUtils::createSortInfo(url, hideFiles);
    } else {
        auto info = fileInfo(url);
        if (info.isNull())
            return nullptr;

        realUrl = info->urlOf(UrlInfoType::kUrl);
        if (!childrenUrlList.contains(realUrl))
            return nullptr;
        sort = sortFileInfo(info);
    }

    if (sort.isNull())
        return nullptr;
    sourceDataList.replace(childrenUrlList.indexOf(realUrl), sort);

    // NOTE: GlobalEventType::kHideFiles event is watched in fileview, but this can be used to notify update view
    // when the file is modified in other way.
    if (UniversalUtils::urlEquals(hiddenFileUrl, url))
        Q_EMIT watcherUpdateHideFile(url);

    return sort;
}

void RootInfoWorker::updateChildren(const QSet<QUrl> &urls)
{
    if (stopped)
        return;
    QList<SortInfoPointer> updates;
    QSet<QString> hideFiles;
    if (url.isLocalFile())
        hideFiles = SortFileInfoUtils::loadHideFileList(this->url);
    std::for_each(urls.begin(), urls.end(), [this, &updates, hideFiles](const QUrl url) {
        if (stopped)
            return;
        auto sort = updateChild(url, hideFiles);
        if (sort)
            updates.append(sort);
    });
    if (updates.isEmpty())
        return;
    emit watcherUpdateFiles(updates);
}

SortInfoPointer RootInfoWorker::addChild(const QUrl &child, const QSet<QString> hidefiles)
{
    auto sort = SortFileInfoUtils::createSortInfo(child, hidefiles);
    if (!sort)
        return nullptr;

    if (childrenUrlList.contains(child)) {
        sourceDataList.replace(childrenUrlList.indexOf(child), sort);
        return sort;
    }

    childrenUrlList.append(child);
    sourceDataList.append(sort);

    return sort;
}

void RootInfoWorker::onResetData()
{
    childrenUrlList.clear();
    sourceDataList.clear();
}

void RootInfoWorker::onSetIteratorStatus(const RootInfoWorker::IteratorStatus &status)
{
    itStatus = status;
}

RootInfo::RootInfo(const QUrl &u, const bool canCache, QObject *parent)
    : QObject(parent), url(u), canCache(canCache)
{
    fmInfo() << "RootInfo created for url:" << u << "canCache=" << canCache;
    if (!url.path().isEmpty() && url.path() != "/")
        url = url.adjusted(QUrl::StripTrailingSlash);

    rootWorker.reset(new RootInfoWorker(url));

    initConnection();
    rootWorker->moveToThread(&rootThread);
    rootWorker->watcherWorker()->moveToThread(&rootThread);
    rootWorker->iteratorWorker()->moveToThread(&rootThread);

    keyWords = KeywordExtractorManager::instance().extractor().extractFromUrl(url);

    connect(qApp, &QApplication::aboutToQuit, this, [this]{
        // 保证所有的迭代线程退出
        this->clearAllThread();
    });
    rootThread.start();
}

RootInfo::~RootInfo()
{
    fmInfo() << "RootInfo destroyed for url:" << url;
    clearAllThread();
}

bool RootInfo::initThreadOfFileData(const QString &key, DFMGLOBAL_NAMESPACE::ItemRoles role, Qt::SortOrder order, bool isMixFileAndFolder)
{
    // clear old dir iterator thread
    for (auto it = discardedThread.begin(); it != discardedThread.end(); ) {
        if (!(*it)->isRunning()) {
            it = discardedThread.erase(it);
        } else {
            it++;
        }
    }
    // create traversal thread
    QSharedPointer<DirIteratorThread> traversalThread = traversalThreads.value(key);
    bool isGetCache = canCache;
    if (!traversalThread.isNull()) {
        traversalThread->traversalThread->disconnect();
    } else {
        isGetCache = (canCache && traversalFinish) || traversaling;
        if (canCache && traversalFinish && isRefresh)
            isGetCache = false;
    }

    traversalThread.reset(new DirIteratorThread);
    traversalThread->traversalThread.reset(
            new TraversalDirThreadManager(url, QStringList(),
                                          QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden,
                                          QDirIterator::FollowSymlinks));
    traversalThread->traversalThread->setSortAgruments(order, role, isMixFileAndFolder);
    traversalThread->traversalThread->setTraversalToken(key);
    initIteratorConnection(traversalThread->traversalThread);
    traversalThreads.insert(key, traversalThread);

    switch (role) {
    case Global::ItemRoles::kItemFileDisplayNameRole:
        traversalThread->originSortRole = dfmio::DEnumerator::SortRoleCompareFlag::kSortRoleCompareFileName;
        break;
    case Global::ItemRoles::kItemFileSizeRole:
        traversalThread->originSortRole = dfmio::DEnumerator::SortRoleCompareFlag::kSortRoleCompareFileSize;
        break;
    case Global::ItemRoles::kItemFileLastReadRole:
        traversalThread->originSortRole = dfmio::DEnumerator::SortRoleCompareFlag::kSortRoleCompareFileLastRead;
        break;
    case Global::ItemRoles::kItemFileLastModifiedRole:
        traversalThread->originSortRole = dfmio::DEnumerator::SortRoleCompareFlag::kSortRoleCompareFileLastModified;
        break;
    default:
        traversalThread->originSortRole = dfmio::DEnumerator::SortRoleCompareFlag::kSortRoleCompareDefault;
    }
    traversalThread->originMixSort = isMixFileAndFolder;
    traversalThread->originSortOrder = order;
    return isGetCache;
}

void RootInfo::startIteratorWork(const QString &key, const bool getCache)
{
    if (!traversalThreads.contains(key)) {
        fmWarning() << "startIteratorWork: key not found" << key;
        return;
    }

    fmInfo() << "startIteratorWork: key=" << key << "getCache=" << getCache << "url=" << url;
    if (getCache)
        return handleGetSourceData(key);

    traversaling = true;
    emit resetData();
    emit iteratorStatus(RootInfoWorker::IteratorStatus::kRunning);
    traversalThreads.value(key)->traversalThread->start();
}

void RootInfo::startWatcher()
{
    if (needStartWatcher == false)
        return;
    needStartWatcher = false;
    if (watcher) {
        watcher->stopWatcher();
        watcher->disconnect(this);
    }

    // create watcher
    watcher = WatcherFactory::create<AbstractFileWatcher>(url);
    if (watcher.isNull()) {
        fmWarning() << "Create watcher failed! url = " << url;
        return;
    }

    // worker 子线程执行
    connect(watcher.data(), &AbstractFileWatcher::fileDeleted,
            rootWorker->watcherWorker().data(), &FileWatcherWorker::doFileDeleted, Qt::QueuedConnection);
    connect(watcher.data(), &AbstractFileWatcher::subfileCreated,
            rootWorker->watcherWorker().data(), &FileWatcherWorker::doFileCreated, Qt::QueuedConnection);
    connect(watcher.data(), &AbstractFileWatcher::fileAttributeChanged,
            rootWorker->watcherWorker().data(), &FileWatcherWorker::doFileUpdated, Qt::QueuedConnection);
    connect(watcher.data(), &AbstractFileWatcher::fileRename,
            rootWorker->watcherWorker().data(), &FileWatcherWorker::doFileMoved, Qt::QueuedConnection);

    watcher->restartWatcher();
}

int RootInfo::clearTraversalThread(const QString &key, const bool isRefresh)
{
    if (!traversalThreads.contains(key))
        return traversalThreads.count();

    auto thread = traversalThreads.take(key);
    auto traversalThread = thread->traversalThread;
    traversalThread->disconnect(this);
    if (traversalThread->isRunning()) {
        discardedThread.append(traversalThread);
        traversaling = false;
    }
    thread->traversalThread->stop();
    if (traversalThreads.isEmpty())
        needStartWatcher = true;

    this->isRefresh = isRefresh;
    return traversalThreads.count();
}

void RootInfo::reset()
{
    disconnect();

    emit resetData();

    if (watcher) {
        watcher->disconnect(this);
        watcher->stopWatcher();
    }

    emit iteratorStatus(RootInfoWorker::IteratorStatus::kNone);

    for (const auto &thread : traversalThreads) {
        thread->traversalThread->stop();
    }
    // wait old dir iterator thread
    for (const auto &thread : discardedThread) {
        thread->disconnect();
        thread->stop();
        thread->quit();
    }
}

bool RootInfo::canDelete() const
{
    for (const auto &thread : traversalThreads) {
        if (!thread->traversalThread->isFinished())
            return false;
    }
    // wait old dir iterator thread
    for (const auto &thread : discardedThread) {
        if (!thread->isFinished())
            return false;
    }
    return true;
}

QStringList RootInfo::getKeyWords() const
{
    return keyWords;
}

void RootInfo::handleTraversalFinish(const QString &travseToken)
{
    fmInfo() << "RootInfo::handleTraversalFinish: token=" << travseToken << "url=" << url << "traversaling=" << traversaling;
    traversaling = false;
    emit traversalFinished(travseToken);
    traversalFinish = true;
    if (isRefresh) {
        fmDebug() << "RootInfo::handleTraversalFinish: refresh completed";
        isRefresh = false;
    }
}

void RootInfo::handleGetSourceData(const QString &currentToken)
{
    if (needStartWatcher)
        startWatcher();

    emit getSourceData(currentToken);
}

void RootInfo::onWatcherTimerStart()
{
    QTimer::singleShot(100, this, [this]{
        emit watcherTimerEvent();
    });
}

void RootInfo::initIteratorConnection(const TraversalThreadManagerPointer &traversalThread)
{
    // 再worker的子线程中执行
    connect(traversalThread.data(), &TraversalDirThreadManager::updateChildrenManager,
            rootWorker->iteratorWorker().data(), &FileIteratorWorker::handleTraversalResults, Qt::QueuedConnection);
    connect(traversalThread.data(), &TraversalDirThreadManager::updateLocalChildren,
            rootWorker->iteratorWorker().data(), &FileIteratorWorker::handleTraversalLocalResult, Qt::QueuedConnection);
    connect(traversalThread.data(), &TraversalDirThreadManager::traversalRequestSort,
            rootWorker->iteratorWorker().data(), &FileIteratorWorker::handleTraversalSort, Qt::QueuedConnection);
    connect(traversalThread.data(), &TraversalDirThreadManager::updateChildrenInfo,
            rootWorker->iteratorWorker().data(), &FileIteratorWorker::handleTraversalResultsUpdate, Qt::QueuedConnection);
    connect(traversalThread.data(), &TraversalDirThreadManager::traversalFinished,
            rootWorker->iteratorWorker().data(), &FileIteratorWorker::handleTraversalFinish, Qt::QueuedConnection);

    // 主线中执行，启动监视器
    connect(traversalThread.data(), &TraversalDirThreadManager::iteratorInitFinished,
            this, &RootInfo::startWatcher, Qt::QueuedConnection);
    // 当前目录下存在损坏的文件
    connect(traversalThread.data(), &TraversalDirThreadManager::traversalFindErrorFile,
            this, &RootInfo::traversalFindErrorFile, Qt::QueuedConnection);
}

void RootInfo::initConnection()
{
    // 连接迭代器信号
    connect(rootWorker.data(), &RootInfoWorker::iteratorLocalFiles, this, &RootInfo::iteratorLocalFiles, Qt::QueuedConnection);
    connect(rootWorker.data(), &RootInfoWorker::iteratorUpdateFiles, this, &RootInfo::iteratorUpdateFiles, Qt::QueuedConnection);
    connect(rootWorker.data(), &RootInfoWorker::iteratorAddFile, this, &RootInfo::iteratorAddFile, Qt::QueuedConnection);
    connect(rootWorker.data(), &RootInfoWorker::iteratorAddFiles, this, &RootInfo::iteratorAddFiles, Qt::QueuedConnection);
    connect(rootWorker.data(), &RootInfoWorker::requestSort, this, &RootInfo::requestSort, Qt::QueuedConnection);
    connect(rootWorker.data(), &RootInfoWorker::traversalFinished, this, &RootInfo::handleTraversalFinish, Qt::QueuedConnection);
    connect(rootWorker.data(), &RootInfoWorker::sourceDatas, this, &RootInfo::sourceDatas, Qt::QueuedConnection);

    // 连接监视器信号
    connect(rootWorker.data(), &RootInfoWorker::watcherAddFiles, this, &RootInfo::watcherAddFiles, Qt::QueuedConnection);
    connect(rootWorker.data(), &RootInfoWorker::watcherRemoveFiles, this, &RootInfo::watcherRemoveFiles, Qt::QueuedConnection);
    connect(rootWorker.data(), &RootInfoWorker::watcherUpdateHideFile, this, &RootInfo::watcherUpdateHideFile, Qt::QueuedConnection);
    connect(rootWorker.data(), &RootInfoWorker::watcherUpdateFile, this, &RootInfo::watcherUpdateFile, Qt::QueuedConnection);
    connect(rootWorker.data(), &RootInfoWorker::watcherUpdateFiles, this, &RootInfo::watcherUpdateFiles, Qt::QueuedConnection);

    // 连接root处理信号
    connect(rootWorker.data(), &RootInfoWorker::requestCloseTab, this, &RootInfo::requestCloseTab, Qt::QueuedConnection);
    connect(rootWorker.data(), &RootInfoWorker::requestClearRoot, this, &RootInfo::requestClearRoot, Qt::QueuedConnection);

    connect(this, &RootInfo::getSourceData, rootWorker->iteratorWorker().data(), &FileIteratorWorker::handleGetSourceData, Qt::QueuedConnection);
    connect(this, &RootInfo::resetData, rootWorker.data(), &RootInfoWorker::onResetData, Qt::QueuedConnection);
    connect(this, &RootInfo::iteratorStatus, rootWorker.data(), &RootInfoWorker::onSetIteratorStatus, Qt::QueuedConnection);
    connect(rootWorker.data(), &RootInfoWorker::watcherTimerStart, this, &RootInfo::onWatcherTimerStart, Qt::QueuedConnection);
    connect(this, &RootInfo::watcherTimerEvent, rootWorker->watcherWorker().data(), &FileWatcherWorker::doWatcherEvent, Qt::QueuedConnection);
}

void RootInfo::clearAllThread()
{
    disconnect();
    if (watcher)
        watcher->stopWatcher();

    for (const auto &thread : traversalThreads) {
        thread->traversalThread->stop();
        thread->traversalThread->wait();
    }
    // wait old dir iterator thread
    for (const auto &thread : discardedThread) {
        thread->disconnect();
        thread->stop();
        thread->quit();
        thread->wait();
    }

    if (rootWorker)
        rootWorker->stop();

    rootThread.quit();
    rootThread.wait();
}
