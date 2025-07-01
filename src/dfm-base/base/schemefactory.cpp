// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <dfm-base/base/schemefactory.h>
#include <dfm-base/utils/fileutils.h>

namespace dfmbase {

InfoFactory *InfoFactory::instance()
{
    // 静态变量再堆上分配，最后析构不会有顺序问题
    static InfoFactory *ins = new InfoFactory;
    return ins;
}

QString InfoFactory::scheme(const QUrl &url)
{
    auto scheme = url.scheme();
    if (scheme != Global::Scheme::kFile)
        return scheme;

    if (!FileUtils::isLocalDevice(url))
        return Global::Scheme::kAsyncFile;

    auto targetPath = FileUtils::symlinkTarget(url);

    if (!targetPath.isEmpty() && !FileUtils::isLocalDevice(QUrl::fromLocalFile(targetPath)))
        scheme = Global::Scheme::kAsyncFile;

    return scheme;
}

QSharedPointer<FileInfo> InfoFactory::getFileInfoFromCache(const QUrl &url, Global::CreateFileInfoType type, QString *errorString)
{
    QSharedPointer<FileInfo> info = InfoCacheController::instance().getCacheInfo(url);
    if (!info) {
        if (type == Global::CreateFileInfoType::kCreateFileInfoSyncAndCache) {
            info = instance()->SchemeFactory<FileInfo>::create(url, errorString);
        } else if (type == Global::CreateFileInfoType::kCreateFileInfoAsyncAndCache) {
            info = instance()->SchemeFactory<FileInfo>::create(Global::Scheme::kAsyncFile, url, errorString);
            if (info) {
                info->refresh();
            }
        }
        if (info)
            emit InfoCacheController::instance().cacheFileInfo(url, info);
    }
    return info;
}

WatcherFactory *WatcherFactory::instance()
{
    // 静态变量再堆上分配，最后析构不会有顺序问题
    static WatcherFactory *ins = new WatcherFactory;
    return ins;
}

DirIteratorFactory *DirIteratorFactory::instance()
{
    // 静态变量再堆上分配，最后析构不会有顺序问题
    static DirIteratorFactory *ins = new DirIteratorFactory;
    return ins;
}

ViewFactory *ViewFactory::instance()
{
    // 静态变量再堆上分配，最后析构不会有顺序问题
    static ViewFactory *ins = new ViewFactory;
    return ins;
}

SortFilterFactory *SortFilterFactory::instance()
{
    // 静态变量再堆上分配，最后析构不会有顺序问题
    static SortFilterFactory *ins = new SortFilterFactory;
    return ins;
}

}   // namespace dfmbase
