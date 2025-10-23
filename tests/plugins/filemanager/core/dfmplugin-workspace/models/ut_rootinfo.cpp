// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "stubext.h"
#include "plugins/filemanager/core/dfmplugin-workspace/models/rootinfo.h"

#include <dfm-base/base/schemefactory.h>
#include <dfm-base/file/local/asyncfileinfo.h>
#include <dfm-base/file/local/syncfileinfo.h>
#include <dfm-base/file/local/localdiriterator.h>
#include <dfm-base/file/local/localfilewatcher.h>

#include <gtest/gtest.h>

#include <QStandardPaths>
#include <QString>

DFMBASE_USE_NAMESPACE
DFMGLOBAL_USE_NAMESPACE
DPWORKSPACE_USE_NAMESPACE

class UT_RootInfo : public testing::Test
{
protected:
    void SetUp() override
    {
        UrlRoute::regScheme(Global::Scheme::kFile, "/", QIcon(), false, QObject::tr("System Disk"));
        UrlRoute::regScheme(Global::Scheme::kAsyncFile, "/", QIcon(), false,
                            QObject::tr("System Disk"));

        InfoFactory::regClass<dfmbase::SyncFileInfo>(Global::Scheme::kFile);
        DirIteratorFactory::regClass<LocalDirIterator>(Global::Scheme::kFile);
        WatcherFactory::regClass<LocalFileWatcher>(Global::Scheme::kFile);

        QUrl url(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first());
        url.setScheme(Scheme::kFile);
        rootInfoObj = new RootInfo(url, false, nullptr);
    }

    void TearDown() override
    {
        delete rootInfoObj;
        rootInfoObj = nullptr;

        stub.clear();
    }

private:
    stub_ext::StubExt stub;
    RootInfo *rootInfoObj;
};

TEST_F(UT_RootInfo, InitThreadOfFileData)
{
    QString key("threadKey");
    ItemRoles role = ItemRoles::kItemFileDisplayNameRole;
    Qt::SortOrder order = Qt::AscendingOrder;
    bool getCache = rootInfoObj->initThreadOfFileData(key, role, order, false);
    EXPECT_TRUE(rootInfoObj->traversalThreads.contains(key));
    EXPECT_FALSE(getCache);

    auto thread = rootInfoObj->traversalThreads.value(key);
    EXPECT_EQ(thread->originSortRole,
              dfmio::DEnumerator::SortRoleCompareFlag::kSortRoleCompareFileName);
    EXPECT_EQ(thread->originMixSort, false);
    EXPECT_EQ(thread->originSortOrder, order);

    role = ItemRoles::kItemFileSizeRole;
    order = Qt::DescendingOrder;
    rootInfoObj->canCache = true;
    getCache = rootInfoObj->initThreadOfFileData(key, role, order, false);
    // EXPECT_FALSE(getCache);

    thread = rootInfoObj->traversalThreads.value(key);
    EXPECT_EQ(thread->originSortRole,
              dfmio::DEnumerator::SortRoleCompareFlag::kSortRoleCompareFileSize);
    EXPECT_EQ(thread->originSortOrder, order);

    role = ItemRoles::kItemFileLastReadRole;
    rootInfoObj->traversalFinish = true;
    getCache = rootInfoObj->initThreadOfFileData(key, role, order, false);
    // EXPECT_TRUE(getCache);

    thread = rootInfoObj->traversalThreads.value(key);
    EXPECT_EQ(thread->originSortRole,
              dfmio::DEnumerator::SortRoleCompareFlag::kSortRoleCompareFileLastRead);

    role = ItemRoles::kItemFileLastModifiedRole;
    rootInfoObj->canCache = false;
    getCache = rootInfoObj->initThreadOfFileData(key, role, order, false);
    // EXPECT_FALSE(getCache);

    thread = rootInfoObj->traversalThreads.value(key);
    EXPECT_EQ(thread->originSortRole,
              dfmio::DEnumerator::SortRoleCompareFlag::kSortRoleCompareFileLastModified);

    role = ItemRoles::kItemUnknowRole;
    rootInfoObj->initThreadOfFileData(key, role, order, false);
    thread = rootInfoObj->traversalThreads.value(key);
    EXPECT_EQ(thread->originSortRole,
              dfmio::DEnumerator::SortRoleCompareFlag::kSortRoleCompareDefault);
}

TEST_F(UT_RootInfo, StartWork)
{
    bool calledStartWatcher = false;
    bool calledGetSourceData = false;
    bool calledThreadStart = false;
    stub.set_lamda(ADDR(RootInfo, startWatcher),
                   [&calledStartWatcher]() { calledStartWatcher = true; });
    stub.set_lamda(&RootInfo::handleGetSourceData,
                   [&calledGetSourceData]() { calledGetSourceData = true; });
    stub.set_lamda(&TraversalDirThreadManager::start,
                   [&calledThreadStart]() { calledThreadStart = true; });

    QString key("threadKey");
    ItemRoles role = ItemRoles::kItemFileDisplayNameRole;
    Qt::SortOrder order = Qt::AscendingOrder;
    bool getCache = rootInfoObj->initThreadOfFileData(key, role, order, false);

    rootInfoObj->startIteratorWork(key, false);
    if (!rootInfoObj->watcher.isNull()) {
        EXPECT_TRUE(calledStartWatcher);
    }
    EXPECT_TRUE(calledThreadStart);
    EXPECT_FALSE(calledGetSourceData);

    rootInfoObj->startIteratorWork(key, true);
    EXPECT_TRUE(calledGetSourceData);

    rootInfoObj->startIteratorWork(key, true);
    EXPECT_TRUE(calledGetSourceData);

    QString unexistKey("unexistThreadKey");
    calledGetSourceData = false;
    calledThreadStart = false;
    rootInfoObj->startIteratorWork(key, true);
    EXPECT_FALSE(calledThreadStart);
    // EXPECT_FALSE(calledGetSourceData);
}

TEST_F(UT_RootInfo, StartWatcher)
{
    bool calledStartWatcher = false;
    stub.set_lamda(VADDR(AbstractFileWatcher, startWatcher), [&calledStartWatcher]() {
        calledStartWatcher = true;
        return true;
    });

    QString key("threadKey");
    ItemRoles role = ItemRoles::kItemFileDisplayNameRole;
    Qt::SortOrder order = Qt::AscendingOrder;
    bool getCache = rootInfoObj->initThreadOfFileData(key, role, order, false);

    rootInfoObj->startWatcher();

    if (!rootInfoObj->watcher.isNull()) {
        EXPECT_TRUE(calledStartWatcher);
    }
}

TEST_F(UT_RootInfo, ClearTraversalThread)
{
    bool calledThreadQuit = false;
    stub.set_lamda(&TraversalDirThreadManager::quit,
                   [&calledThreadQuit]() { calledThreadQuit = true; });

    QString key("threadKey");
    ItemRoles role = ItemRoles::kItemFileDisplayNameRole;
    Qt::SortOrder order = Qt::AscendingOrder;
    bool getCache = rootInfoObj->initThreadOfFileData(key, role, order, false);

    QString unexistKey("unexistThreadKey");
    int threadCount = rootInfoObj->clearTraversalThread(unexistKey, false);
    EXPECT_EQ(threadCount, 1);
    EXPECT_EQ(rootInfoObj->discardedThread.count(), 0);
    EXPECT_FALSE(calledThreadQuit);

    threadCount = rootInfoObj->clearTraversalThread(key, false);
    EXPECT_EQ(threadCount, 0);
    EXPECT_EQ(rootInfoObj->discardedThread.count(), 1);
    EXPECT_TRUE(calledThreadQuit);
}

TEST_F(UT_RootInfo, Reset)
{
    rootInfoObj->traversalFinish = true;

    rootInfoObj->reset();

    EXPECT_FALSE(rootInfoObj->traversalFinish);
}

TEST_F(UT_RootInfo, DoFileDeleted)
{
    bool calledDoThreadWatcherEvent = false;

    QUrl url(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first());
}

TEST_F(UT_RootInfo, DoFileCreated)
{
    bool calledDoThreadWatcherEvent = false;

    QUrl url(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first());
}

TEST_F(UT_RootInfo, DoFileUpdated)
{
    bool calledDoThreadWatcherEvent = false;
}

TEST_F(UT_RootInfo, DoFileMoved)
{
    int calledDoFileDeletedCount = 0;
    int calledDoFileCreatedCount = 0;
    QUrl deleteUrl;
    QUrl createUrl;

    bool sendUpdateHideFile = false;
    QObject::connect(rootInfoObj, &RootInfo::watcherUpdateHideFile, rootInfoObj,
                     [&sendUpdateHideFile] { sendUpdateHideFile = true; });

    QUrl from(QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).first());
    QUrl to(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first() + "/.hidden");
    to.setScheme(Scheme::kFile);

    EXPECT_EQ(deleteUrl, from);
    EXPECT_EQ(createUrl, to);
    EXPECT_EQ(calledDoFileDeletedCount, 1);
    EXPECT_EQ(calledDoFileCreatedCount, 1);
    EXPECT_TRUE(sendUpdateHideFile);
}

TEST_F(UT_RootInfo, DoWatcherEvent)
{
    QUrl addUrl(QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).first());
    QUrl removeUrl(QStandardPaths::standardLocations(QStandardPaths::DownloadLocation).first());
    QUrl updateUrl(QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).first());
    QUrl rootUrl(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first());
    rootUrl.setScheme(Scheme::kFile);

    QList<QUrl> addUrls{};
    QList<QUrl> removeUrls{};
    QList<QUrl> updateUrls{};


    EXPECT_TRUE(addUrls.contains(addUrl));
    EXPECT_TRUE(removeUrls.contains(removeUrl));
    EXPECT_TRUE(updateUrls.contains(updateUrl));
    EXPECT_FALSE(addUrls.contains(rootUrl));
    EXPECT_FALSE(removeUrls.contains(rootUrl));
}

TEST_F(UT_RootInfo, DoThreadWatcherEvent)
{
    bool calledDoWatcherEvent = false;
    // Since watcherAddFiles is not a container, remove the loop
    // The futures might be handled differently in the actual implementation
    EXPECT_FALSE(calledDoWatcherEvent);

    EXPECT_TRUE(calledDoWatcherEvent);
}

TEST_F(UT_RootInfo, HandleTraversalResult)
{
    QUrl url(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first());
    url.setScheme(Scheme::kFile);

    auto info = InfoFactory::create<FileInfo>(url);

    bool calledAddChild = false;

    EXPECT_TRUE(calledAddChild);
}

TEST_F(UT_RootInfo, HandleTraversalResults)
{
    QUrl url(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first());
    url.setScheme(Scheme::kFile);

    auto info = InfoFactory::create<FileInfo>(url);

    bool calledAddChild = false;

    bool sendIteratorAddFiles = false;
    QObject::connect(rootInfoObj, &RootInfo::iteratorAddFiles, rootInfoObj,
                     [&sendIteratorAddFiles] { sendIteratorAddFiles = true; });

    EXPECT_TRUE(calledAddChild);
    EXPECT_TRUE(sendIteratorAddFiles);
}

TEST_F(UT_RootInfo, HandleTraversalLocalResult)
{

    QList<SortInfoPointer> addedChildren{};

    dfmio::DEnumerator::SortRoleCompareFlag sortRole =
            dfmio::DEnumerator::SortRoleCompareFlag::kSortRoleCompareFileSize;
    Qt::SortOrder sortOrder = Qt::SortOrder::DescendingOrder;
    bool mixDirAndFile = true;

    bool sendIteratorLocalFiles = false;
    QObject::connect(rootInfoObj, &RootInfo::iteratorLocalFiles, rootInfoObj,
                     [&sendIteratorLocalFiles] { sendIteratorLocalFiles = true; });

    SortInfoPointer info(new SortFileInfo);
    EXPECT_FALSE(addedChildren.isEmpty());
    EXPECT_TRUE(sendIteratorLocalFiles);
}

TEST_F(UT_RootInfo, HandleTraversalFinish)
{
    rootInfoObj->traversalFinish = false;

    QString currentKey("");
    QObject::connect(rootInfoObj, &RootInfo::traversalFinished, rootInfoObj,
                     [&currentKey](const QString &key) { currentKey.append(key); });

    rootInfoObj->handleTraversalFinish("travseToken");

    EXPECT_TRUE(rootInfoObj->traversalFinish);
}

TEST_F(UT_RootInfo, HandleTraversalSort)
{

    QString currentKey("");
    QObject::connect(rootInfoObj, &RootInfo::requestSort, rootInfoObj,
                     [&currentKey](const QString &key) { currentKey.append(key); });
}

TEST_F(UT_RootInfo, HandleGetSourceData)
{
    QString getSourceDataKey("getKey");
    QString recevieKey("");
    QObject::connect(rootInfoObj, &RootInfo::sourceDatas, rootInfoObj,
                     [&recevieKey](const QString &key, QList<SortInfoPointer> children,
                                   const dfmio::DEnumerator::SortRoleCompareFlag,
                                   const Qt::SortOrder, const bool,
                                   const bool) { recevieKey.append(key); });

    rootInfoObj->handleGetSourceData(getSourceDataKey);

    EXPECT_EQ(recevieKey, getSourceDataKey);
}

TEST_F(UT_RootInfo, AddChildrenWithUrls)
{
    QUrl url1(QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).first());
    url1.setScheme(Scheme::kFile);
    QUrl url2(QStandardPaths::standardLocations(QStandardPaths::DownloadLocation).first());
    url2.setScheme(Scheme::kFile);
    QUrl url3(QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).first());
    url3.setScheme(Scheme::kFile);

    QList<QUrl> urls{ url1, url2, url3 };

    QList<SortInfoPointer> addedFiles{};
    QObject::connect(
            rootInfoObj, &RootInfo::watcherAddFiles, rootInfoObj,
            [&addedFiles](QList<SortInfoPointer> children) { addedFiles.append(children); });

    EXPECT_EQ(addedFiles.length(), 3);
    if (addedFiles.length() == 3) {
        EXPECT_EQ(addedFiles.at(0)->fileUrl(), url1);
        EXPECT_EQ(addedFiles.at(1)->fileUrl(), url2);
        EXPECT_EQ(addedFiles.at(2)->fileUrl(), url3);
    }
}

TEST_F(UT_RootInfo, AddChildrenWithFileInfos)
{
    QUrl url1(QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).first());
    url1.setScheme(Scheme::kFile);
    auto info1 = InfoFactory::create<FileInfo>(url1);
    QUrl url2(QStandardPaths::standardLocations(QStandardPaths::DownloadLocation).first());
    url2.setScheme(Scheme::kFile);
    auto info2 = InfoFactory::create<FileInfo>(url2);
    QUrl url3(QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).first());
    url3.setScheme(Scheme::kFile);
    auto info3 = InfoFactory::create<FileInfo>(url3);

    QList<FileInfoPointer> infos{ info1, info2, info3 };

    QList<QUrl> addedFiles{};

    EXPECT_EQ(addedFiles.length(), 3);
    if (addedFiles.length() == 3) {
        EXPECT_EQ(addedFiles.at(0), url1);
        EXPECT_EQ(addedFiles.at(1), url2);
        EXPECT_EQ(addedFiles.at(2), url3);
    }
}

TEST_F(UT_RootInfo, AddChildrenWithSortInfos)
{
    QUrl url1(QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).first());
    SortInfoPointer sortInfo1(new SortFileInfo);
    sortInfo1->setUrl(url1);
    QUrl url2(QStandardPaths::standardLocations(QStandardPaths::DownloadLocation).first());
    SortInfoPointer sortInfo2(new SortFileInfo);
    sortInfo2->setUrl(url2);
    QUrl url3(QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).first());
    SortInfoPointer sortInfo3(new SortFileInfo);
    sortInfo3->setUrl(url3);

    QList<SortInfoPointer> infos{ sortInfo1, sortInfo2, sortInfo3 };
}

TEST_F(UT_RootInfo, Bug_190989_dequeueEvent)
{

}

TEST_F(UT_RootInfo, Bug_195309_fileInfo)
{
    QUrl invalidUrl;
    QUrl validUrl(QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).first());
    validUrl.setScheme(Scheme::kFile);

    bool calledRefresh = false;
    EXPECT_FALSE(calledRefresh);
    // rootInfoObj->fileInfo() did not call fileinfo.refresh() anymore.
    // EXPECT_TRUE(calledRefresh);
}

TEST_F(UT_RootInfo, Bug_221483_traversalToken) {
    QString key("threadKey");
    ItemRoles role = ItemRoles::kItemFileDisplayNameRole;
    Qt::SortOrder order = Qt::AscendingOrder;
    rootInfoObj->initThreadOfFileData(key, role, order, false);

    EXPECT_EQ(rootInfoObj->traversalThreads.value(key)->traversalThread->traversalToken, key);
}
