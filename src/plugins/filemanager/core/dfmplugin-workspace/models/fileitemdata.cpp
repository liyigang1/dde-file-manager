// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fileitemdata.h"

#include <dfm-base/dfm_global_defines.h>
#include <dfm-base/base/schemefactory.h>
#include <dfm-base/utils/fileutils.h>
#include <dfm-base/utils/thumbnail/thumbnailfactory.h>

#include <QStandardPaths>

using namespace dfmbase;
using namespace dfmbase::Global;
using namespace dfmplugin_workspace;

FileItemData::FileItemData(const QUrl &url, const FileInfoPointer &info, FileItemData *parent)
    : QObject(),
      parent(parent),
      url(url),
      info(info)
{
    if (!info) {
        QPointer<FileItemData> me = this;
        QtConcurrent::run([=]{
            auto tmInfo  = InfoFactory::create<FileInfo>(url);
            if (me)
                me->info = tmInfo;
        });
    }
    if (info) {
        info->customData(kItemFileRefreshIcon);
        updateOnce = !info->extendAttributes(ExtInfoType::kFileNeedUpdate).toBool();
    }
}

FileItemData::FileItemData(const SortInfoPointer &info, FileItemData *parent)
    : QObject(),
      parent(parent),
      url(info->fileUrl()),
      sortInfo(info)
{
}

void FileItemData::setParentData(FileItemData *p)
{
    parent = p;
}

void FileItemData::setSortFileInfo(SortInfoPointer info)
{
    sortInfo = info;
}

void FileItemData::refreshInfo()
{
    if (!info.isNull())
        info->refresh();
}

void FileItemData::clearThumbnail()
{
    if (!info.isNull())
        info->setExtendedAttributes(ExtInfoType::kFileThumbnail, QVariant());
}

FileInfoPointer FileItemData::fileInfo() const
{
    return info;
}

FileItemData *FileItemData::parentData() const
{
    return parent;
}

QIcon FileItemData::fileIcon() const
{
    if (!info)
        return QIcon::fromTheme("empty");

    const auto &vaule = info->extendAttributes(ExtInfoType::kFileThumbnail);
    if (!vaule.isValid()) {
        ThumbnailFactory::instance()->joinThumbnailJob(url, Global::kLarge);
        // make sure the thumbnail is generated only once
        info->setExtendedAttributes(ExtInfoType::kFileThumbnail, QIcon());
    } else {
        const auto &thumbIcon = vaule.value<QIcon>();
        if (!thumbIcon.isNull())
            return thumbIcon;
    }

    return info->fileIcon();
}

QVariant FileItemData::data(int role) const
{
    if (info) {
        auto val = info->customData(role);
        if (val.isValid())
            return val;
    }

    switch (role) {
    case kItemCreateFileInfoRole:
        assert(qApp->thread() == QThread::currentThread());
        if (info.isNull()) {
            const_cast<FileItemData *>(this)->info = InfoFactory::create<FileInfo>(url);
            if (info) {
                info->customData(kItemFileRefreshIcon);
                if (FileUtils::isLocalDevice(info->fileUrl()))
                    info->updateAttributes();
            }
        } else if (!updateOnce) {
            updateOnce = true;
            info->setExtendedAttributes(ExtInfoType::kFileNeedUpdate, false);
            info->updateAttributes();
            if (info->extendAttributes(ExtInfoType::kFileLocalDevice).toBool())
                const_cast<FileItemData *>(this)->transFileInfo();
        }
        return QVariant();
    case kItemFilePathRole:
        if (info)
            return info->displayOf(DisPlayInfoType::kFileDisplayPath);
        return url.path();
    case kItemFileLastModifiedRole: {
        if (info) {
            auto lastModified = info->timeOf(TimeInfoType::kLastModified).value<QDateTime>();
            return lastModified.isValid() ? lastModified.toString(FileUtils::dateTimeFormat()) : "-";
        }
        return "-";
    }
    case kItemIconRole:
        return fileIcon();
    case kItemFileSizeRole:
        return fileDisplaySize();
    case kItemFileMimeTypeRole:
        if (info)
            return info->displayOf(DisPlayInfoType::kMimeTypeDisplayName);
        return QString();
    case kItemSizeHintRole:
        return QSize(-1, 26);
    case kItemNameRole:
        if (info)
            return info->nameOf(NameInfoType::kFileName);
        return url.fileName();
    case Qt::DisplayRole:
    case kItemEditRole:
    case kItemFileDisplayNameRole:
        return getFileDisplayName();
    case kItemFileLastReadRole:
        if (info)
            return info->customData(dfmbase::Global::kItemFileLastReadRole);
        return QString();
    case kItemFilePinyinNameRole:
        if (info)
            return info->displayOf(DisPlayInfoType::kFileDisplayPinyinName);
        return url.fileName();
    case kItemFileBaseNameRole:
        if (info)
            return info->nameOf(NameInfoType::kCompleteBaseName);
        return url.fileName();
    case kItemFileSuffixRole:
        if (info)
            return info->nameOf(NameInfoType::kSuffix);
        return url.fileName();
    case kItemFileNameOfRenameRole:
        if (info)
            return info->nameOf(NameInfoType::kFileNameOfRename);
        return url.fileName();
    case kItemFileBaseNameOfRenameRole:
        if (info)
            return info->nameOf(NameInfoType::kBaseNameOfRename);
        return url.fileName();
    case kItemFileSuffixOfRenameRole:
        if (info)
            return info->nameOf(NameInfoType::kSuffixOfRename);
        return url.fileName();
    case kItemUrlRole:
        if (info)
            return info->urlOf(UrlInfoType::kUrl);
        return url;
    case Qt::TextAlignmentRole:
        return Qt::AlignVCenter;
    case kItemFileIconModelToolTipRole: {
        const QString filePath = data(kItemFilePathRole).toString();
        const QString stdDocPath = QStandardPaths::writableLocation(QStandardPaths::StandardLocation::DocumentsLocation);
        const QString stdDownPath = QStandardPaths::writableLocation(QStandardPaths::StandardLocation::DownloadLocation);
        const QString stdDataDocPath = FileUtils::bindPathTransform(stdDocPath, true);
        const QString stdDataDownPath = FileUtils::bindPathTransform(stdDownPath, true);
        if (filePath == stdDocPath || filePath == stdDownPath || filePath == stdDataDocPath || filePath == stdDataDownPath)
            return QString();
        QString strToolTip = data(kItemFileDisplayNameRole).toString();
        return strToolTip;
    }
    case kItemFileIsWritableRole:
        if (info)
            return info->isAttributes(OptInfoType::kIsWritable);
        if (sortInfo)
            return sortInfo->isWriteable();
        return true;
    case kItemFileIsDirRole:
        if (info)
            return info->isAttributes(OptInfoType::kIsDir);
        if (sortInfo)
            return sortInfo->isDir();
        return true;
    case kItemFileCanRenameRole:
        if (info)
            return info->canAttributes(CanableInfoType::kCanRename);
        return true;
    case kItemFileCanDropRole:
        if (info)
            return info->canAttributes(CanableInfoType::kCanDrop);
        return true;
    case kItemFileCanDragRole:
        if (info)
            return info->canAttributes(CanableInfoType::kCanDrag);
        return true;
    case kItemFileSizeIntRole:
        return fileSize();
    case kItemFileIsAvailableRole:
        return isAvailable;
    case kItemTreeViewDepthRole:
        return QVariant(depth);
    case kItemTreeViewExpandedRole:
        return QVariant(expanded);
    case kItemTreeViewCanExpandRole:
        return isDir();
    case kItemUpdateAndTransFileInfoRole:
        if (!updateOnce) {
            updateOnce = true;
            info->setExtendedAttributes(ExtInfoType::kFileNeedUpdate, false);
            info->updateAttributes();
            if (info->extendAttributes(ExtInfoType::kFileLocalDevice).toBool())
                const_cast<FileItemData *>(this)->transFileInfo();
        }
        return QVariant();
    case kItemFileContentPreviewRole:
        if (sortInfo)
            return sortInfo->highlightContent();
        return QVariant();
    default:
        return QVariant();
    }
}

void FileItemData::setAvailableState(bool b)
{
    isAvailable = b;
}

void FileItemData::setExpanded(bool b)
{
    expanded = b;
}

void FileItemData::setDepth(const int8_t depth)
{
    this->depth = depth;
}

bool FileItemData::isDir() const
{
    if (info)
        return info->isAttributes(OptInfoType::kIsDir);
    if (sortInfo)
        return sortInfo->isDir();

    return false;
}

QString FileItemData::getFileDisplayName() const
{
    if (info)
        return info->displayOf(DisPlayInfoType::kFileDisplayName);

    // 如果sortInfo有缓存的displayName，直接返回
    if (sortInfo && !sortInfo->displayName().isEmpty())
        return sortInfo->displayName();

    // 不是本地文件直接返回url中的文件名称
    if (!url.isLocalFile() || (sortInfo && sortInfo->isDir()))
        return url.fileName();

    QString displayName;
    // 检查suffix是否是desktop文件，进行处理
    if (FileUtils::isDesktopFileSuffix(url)) {
        try {
            DesktopFile desktopFile(url.path());
            if (desktopFile.desktopDeepinVendor() == QStringLiteral("deepin") &&
                    !(desktopFile.desktopDisplayName().isEmpty())) {
                displayName = desktopFile.desktopDisplayName();
            } else {
                displayName = desktopFile.desktopLocalName().isEmpty() ? displayName : desktopFile.desktopLocalName();
            }
        } catch (...) {
            // 处理桌面文件解析异常
            displayName = url.fileName();
        }
    }

    displayName = displayName.isEmpty() ? url.fileName() : displayName;

    if (sortInfo)
        sortInfo->setDisplayName(displayName);

    return displayName;
}

qint64 FileItemData::fileSize() const
{
    if (info)
        return info->size();

    if (!sortInfo)
        return -1;

    return sortInfo->fileSize();
}

QString FileItemData::fileDisplaySize() const
{
    if (info)
        return info->displayOf(DisPlayInfoType::kSizeDisplayName);

    if (!sortInfo || sortInfo->isDir())
        return "-";

    return FileUtils::formatSize(sortInfo->fileSize());
}

void FileItemData::transFileInfo()
{
    if (info.isNull() ||
            !info->extendAttributes(ExtInfoType::kFileNeedTransInfo).toBool())
        return;
    info->setExtendedAttributes(ExtInfoType::kFileNeedTransInfo, false);
    auto infoTrans = InfoFactory::transfromInfo(url.scheme(), info);
    if (infoTrans != info) {
        info = infoTrans;
        emit InfoCacheController::instance().removeCacheFileInfo({url});
        emit InfoCacheController::instance().cacheFileInfo(url, infoTrans);
    }
}
