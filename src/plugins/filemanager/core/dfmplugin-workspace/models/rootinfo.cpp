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
    // 自己删除可以执行
    if (rootptr->stoped || !isSubFile(fileUrl))
        return;

    FinallyUtil time([this]{
        doCheckAndStartTimer();
    });
    // 删除的是自己
    if (UniversalUtils::urlEquals(fileUrl, rootptr->url)) {
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

void FileWatcherWorker::dofileMoved(const QUrl &fromUrl, const QUrl &toUrl)
{
    assert(qApp->thread() != QThread::currentThread());
    // 处理以前的
    doFileDeleted(fromUrl);

    dofileCreated(toUrl);
}

void FileWatcherWorker::dofileCreated(const QUrl &fileUrl)
{
    assert(qApp->thread() != QThread::currentThread());
    if (rootptr->stoped || !isSubFile(fileUrl))
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
    if (rootptr->stoped || !isSubFile(fileUrl) || UniversalUtils::urlEquals(fileUrl, rootptr->url))
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
    dalayTimeStart = false;
    doWatcherSubEvent();
}

void FileWatcherWorker::doWatcherSubEvent()
{
    if (rootptr->stoped)
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
    if (dalayTimeStart)
        return;
    dalayTimeStart = true;
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
    if (rootptr->stoped)
        return;

    QList<SortInfoPointer> sortInfos;
    QList<FileInfoPointer> infos;
    std::for_each(children.begin(), children.end(), [this, &sortInfos, &infos](const FileInfoPointer &info){
        if (rootptr->stoped)
            return;
        auto sortInfo = rootptr->addChild(info);
        if (!sortInfo)
            return ;
        sortInfos.append(sortInfo);

        infos.append(info);
    });

    if (sortInfos.length() > 0)
        Q_EMIT rootptr->iteratorAddFiles(travseToken, sortInfos, infos);
}

// 处理search的后面的搜索结果
void FileIteratorWorker::handleTraversalResultsUpdate(const QList<SortInfoPointer> &children, const QString &travseToken)
{
    assert(qApp->thread() != QThread::currentThread());
    if (children.isEmpty() || rootptr->stoped)
        return;

    rootptr->addChildren(children);

    emit rootptr->iteratorUpdateFiles(travseToken, rootptr->sourceDataList, false);
}

// 处理搜索的第一次推送的结果或者迭代器使用sortFileInfoList迭代出来的文件
void FileIteratorWorker::handleTraversalLocalResult(QList<SortInfoPointer> children, dfmio::DEnumerator::SortRoleCompareFlag sortRole, Qt::SortOrder sortOrder, bool isMixDirAndFile, const QString &travseToken)
{
    assert(qApp->thread() != QThread::currentThread());
    if (rootptr->stoped)
        return;
    rootptr->originSortRole = sortRole;
    rootptr->originSortOrder = sortOrder;
    rootptr->originMixSort = isMixDirAndFile;

    rootptr->addChildren(children);

    Q_EMIT rootptr->iteratorLocalFiles(travseToken,rootptr->sourceDataList, rootptr->originSortRole, rootptr->originSortOrder, rootptr->originMixSort, true);
}

void FileIteratorWorker::handleTraversalFinish(const QString &travseToken)
{
    if (rootptr->stoped)
        return;
    rootptr->itStatus = RootInfoWorker::IteratorStatus::kFinshed;
    emit rootptr->traversalFinished(travseToken);
}

void FileIteratorWorker::handleTraversalSort(const QString &travseToken)
{
    if (rootptr->stoped)
        return;
    emit rootptr->requestSort(travseToken, rootptr->url);
}

void FileIteratorWorker::handleGetSourceData(const QString &currentToken)
{
    if (rootptr->stoped)
        return;

    QList<SortInfoPointer> newDatas = rootptr->sourceDataList;

    emit rootptr->sourceDatas(currentToken, newDatas, rootptr->originSortRole, rootptr->originSortOrder, rootptr->originMixSort, rootptr->itStatus == RootInfoWorker::IteratorStatus::kFinshed);
    if (rootptr->itStatus == RootInfoWorker::IteratorStatus::kFinshed) {
        emit rootptr->requestSort(currentToken, rootptr->url);
        emit rootptr->traversalFinished(currentToken);
    }
}

RootInfoWorker::RootInfoWorker(const QUrl &url, QObject *parent)
    : QObject (parent), url(url)
{
    it.reset(new FileIteratorWorker(this));
    watch.reset(new FileWatcherWorker(this));
    keyWords = KeywordExtractorManager::instance().extractor().extractFromUrl(url);
}

RootInfoWorker::~RootInfoWorker()
{

}

void RootInfoWorker::stop()
{
    stoped = true;
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
    if (stoped)
        return;
    QList<SortInfoPointer> newSortInfo;

    bool isContainHidd = false;
    std::for_each(urlList.begin(), urlList.end(),[this, &isContainHidd, &newSortInfo](QUrl url) {
        if (stoped)
            return;
        url.setPath(url.path());

        auto child = fileInfo(url);

        if (!child)
            return;

        if (UniversalUtils::urlEquals(url, hiddenFileUrl))
            isContainHidd = true;

        auto sortInfo = addChild(child);
        if (sortInfo)
            newSortInfo.append(sortInfo);
    });

    if (newSortInfo.count() > 0) {
        originSortRole = dfmio::DEnumerator::SortRoleCompareFlag::kSortRoleCompareDefault;
        emit watcherAddFiles(newSortInfo);
    }

    if (isContainHidd)
        Q_EMIT watcherUpdateHideFile(hiddenFileUrl);
}

void RootInfoWorker::addChildren(const QList<FileInfoPointer> &children)
{
    if (stoped)
        return;
    std::for_each(children.begin(), children.end(), [this](const FileInfoPointer child){
        if (stoped)
            return;
        addChild(child);
    });
}

void RootInfoWorker::addChildren(const QList<SortInfoPointer> &children, const int start)
{
    // 这里搜索时不能使用增量更新，以前的SortInfo也有修改，所以前面搜索出来的有可能搜索的内容没有显示出来
    Q_UNUSED(start);
    if (stoped)
        return;

    childrenUrlList.clear();
    sourceDataList = children;

    std::for_each(children.begin(), children.end(), [this](const SortInfoPointer file){
        if (!file || stoped)
            return;
        childrenUrlList.append(file->fileUrl());
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
    if (!info || stoped)
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

    // 这里最近访问时间是否加入？ todo
    return sortInfo;
}

void RootInfoWorker::removeChildren(const QSet<QUrl> &urlList)
{
    if (stoped)
        return;
    QList<SortInfoPointer> removeChildren {};
    int childIndex = -1;
    QList<QUrl> removeUrls;
    emit InfoCacheController::instance().removeCacheFileInfo(urlList.toList());
    std::for_each(urlList.begin(), urlList.end(), [this, &removeUrls, &removeChildren, &childIndex](const QUrl &fileUrl){
        if (stoped)
            return;
        auto url = fileUrl;
        url.setPath(url.path());
        auto child = fileInfo(url);
        if (!child)
            return;

        // 处理各个窗口显示的父目录删除
        if (child->isDir()) {
            WatcherCache::instance().removeCacheWatcherByParent(url);
            emit requestCloseTab(url);
        }

        auto realUrl = child->urlOf(UrlInfoType::kUrl);
        removeUrls.append(realUrl);
        childIndex = childrenUrlList.indexOf(realUrl);
        if (childIndex < 0 || childIndex >= childrenUrlList.length()) {
            removeChildren.append(sortFileInfo(child));
            return;
        }
        childrenUrlList.removeAt(childIndex);
        removeChildren.append(sourceDataList.takeAt(childIndex));
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
    if (stoped)
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

SortInfoPointer RootInfoWorker::updateChild(const QUrl &url)
{
    if (stoped)
        return nullptr;
    SortInfoPointer sort { nullptr };

    auto info = fileInfo(url);
    if (info.isNull())
        return nullptr;

    auto realUrl = info->urlOf(UrlInfoType::kUrl);
    if (!childrenUrlList.contains(realUrl))
        return nullptr;
    sort = sortFileInfo(info);
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
    if (stoped)
        return;
    QList<SortInfoPointer> updates;
    std::for_each(urls.begin(), urls.end(), [this, &updates](const QUrl url) {
        if (stoped)
            return;
        auto sort = updateChild(url);
        if (sort)
            updates.append(sort);
    });
    if (updates.isEmpty())
        return;
    emit watcherUpdateFiles(updates);
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
    if (!traversalThreads.contains(key))
        return;

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
            rootWorker->watcherWorker().data(), &FileWatcherWorker::dofileCreated, Qt::QueuedConnection);
    connect(watcher.data(), &AbstractFileWatcher::fileAttributeChanged,
            rootWorker->watcherWorker().data(), &FileWatcherWorker::doFileUpdated, Qt::QueuedConnection);
    connect(watcher.data(), &AbstractFileWatcher::fileRename,
            rootWorker->watcherWorker().data(), &FileWatcherWorker::dofileMoved, Qt::QueuedConnection);

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
    traversaling = false;
    emit traversalFinished(travseToken);
    traversalFinish = true;
    if (isRefresh) {
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
