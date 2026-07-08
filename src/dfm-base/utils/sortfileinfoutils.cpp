// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sortfileinfoutils.h"

#include <dfm-base/interfaces/sortfileinfo.h>
#include <dfm-base/utils/fileutils.h>

#include <dfm-io/dfmio_utils.h>

#include <QUrl>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

DFMBASE_USE_NAMESPACE

QSet<QString> SortFileInfoUtils::loadHideFileList(const QUrl &dir)
{
    const QUrl &url = DFMIO::DFMUtils::buildFilePath(dir.toString().toUtf8().data(), ".hidden", nullptr);
    return  DFMIO::DFMUtils::hideListFromUrl(url);
}

SortInfoPointer SortFileInfoUtils::createSortInfo(const QString &parentPath,
                                                  const QString &fileName,
                                                  const QSet<QString> &hideList)
{
    const QString entryPath = QDir(parentPath).filePath(fileName);
    return createSortInfo(entryPath, hideList);
}

SortInfoPointer SortFileInfoUtils::createSortInfo(const QString &entryPath,
                                                  const QSet<QString> &hideList)
{
    if (entryPath.isEmpty())
        return nullptr;

    auto fileName = QUrl::fromLocalFile(entryPath).fileName();

    const QByteArray nativePath = QFile::encodeName(entryPath);

    // 使用 statx 获取所有文件属性（包括创建时间 birth time）
    struct statx stx;
    unsigned int mask = STATX_BASIC_STATS | STATX_BTIME;
    if (statx(AT_FDCWD, nativePath.constData(), AT_SYMLINK_NOFOLLOW | AT_NO_AUTOMOUNT, mask, &stx) != 0)
        return nullptr;

    const bool isSymLink = S_ISLNK(stx.stx_mode);
    mode_t effectiveMode = stx.stx_mode;
    uint64_t effectiveSize = stx.stx_size;
    time_t effectiveAtime = stx.stx_atime.tv_sec;
    time_t effectiveMtime = stx.stx_mtime.tv_sec;
    time_t effectiveBtime = (stx.stx_mask & STATX_BTIME) ? stx.stx_btime.tv_sec : 0;

    SortInfoPointer info(new SortFileInfo);
    info->setUrl(QUrl::fromLocalFile(entryPath));
    info->setSymlink(isSymLink);
    if (isSymLink) {
        const QUrl entryUrl = QUrl::fromLocalFile(entryPath);
        const QString symlinkTarget = FileUtils::symlinkTarget(entryUrl);
        if (!symlinkTarget.isEmpty())
            info->setSymlinkTarget(QUrl::fromLocalFile(symlinkTarget));

        const QString targetPath = FileUtils::resolveSymlink(entryUrl);
        if (!targetPath.isEmpty() && FileUtils::isLocalDevice(QUrl::fromLocalFile(targetPath))) {
            const QByteArray targetNativePath = QFile::encodeName(targetPath);
            struct statx targetStx;
            if (statx(AT_FDCWD, targetNativePath.constData(), 0, mask, &targetStx) == 0) {
                effectiveMode = targetStx.stx_mode;
                effectiveSize = targetStx.stx_size;
                effectiveAtime = targetStx.stx_atime.tv_sec;
                effectiveMtime = targetStx.stx_mtime.tv_sec;
                if (targetStx.stx_mask & STATX_BTIME)
                    effectiveBtime = targetStx.stx_btime.tv_sec;
            }
        }
    }
    info->setSize(static_cast<qint64>(effectiveSize));
    info->setDir(S_ISDIR(effectiveMode));
    info->setFile(!S_ISDIR(effectiveMode));
    info->setHide(fileName.startsWith(".") || hideList.contains(fileName));
    info->setReadable((effectiveMode & S_IRUSR) != 0);
    info->setWriteable((effectiveMode & S_IWUSR) != 0);
    info->setExecutable((effectiveMode & S_IXUSR) != 0);
    info->setLastReadTime(effectiveAtime);
    info->setLastModifiedTime(effectiveMtime);
    info->setCreateTime(effectiveBtime);
    info->setInfoCompleted(true);
    return info;
}

void SortFileInfoUtils::cacheLastReadTime(SortInfoPointer &sortInfo, const FileInfoPointer &info)
{
    auto lastRead = info->customData(Global::ItemRoles::kItemFileLastReadRole).value<QDateTime>().toMSecsSinceEpoch();
    if (lastRead > 0) {
        sortInfo->setLastReadTime(lastRead);
    } else {
        sortInfo->setLastReadTime(info->timeOf(TimeInfoType::kLastRead).toLongLong());
    }
}

SortInfoPointer SortFileInfoUtils::createSortInfo(const QUrl &url,
                                                  const QSet<QString> &hideList)
{
    if (!url.isLocalFile())
        return nullptr;

    return createSortInfo(url.path(), hideList);
}
