// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <dfm-base/interfaces/private/sortfileinfo_p.h>
#include <dfm-base/utils/fileutils.h>

#include <QtConcurrent>
#include <QMutexLocker>

#include <sys/stat.h>

namespace dfmbase {

SortFileInfo::SortFileInfo()
    : d(new SortFileInfoPrivate(this))
{
}

SortFileInfo::~SortFileInfo()
{
}

void SortFileInfo::setUrl(const QUrl &url)
{
    d->url = url;
    d->getDisplayName();
}

void SortFileInfo::setSize(const qint64 size)
{
    d->filesize = size;
}

void SortFileInfo::setFile(const bool isfile)
{
    d->file = isfile;
}

void SortFileInfo::setDir(const bool isdir)
{
    d->dir = isdir;
}

void SortFileInfo::setSymlink(const bool isSymlink)
{
    d->symLink = isSymlink;
}

void SortFileInfo::setHide(const bool ishide)
{
    d->hide = ishide;
}

void SortFileInfo::setReadable(const bool readable)
{
    d->readable = readable;
}

void SortFileInfo::setWriteable(const bool writeable)
{
    d->writeable = writeable;
}

void SortFileInfo::setExecutable(const bool executable)
{
    d->executable = executable;
}

void SortFileInfo::setSymlinkTarget(const QUrl &url)
{
    d->symLinkTag = url;
}

void SortFileInfo::setLastReadTime(const qint64 time)
{
    d->lastRead = time;
}

void SortFileInfo::setLastModifiedTime(const qint64 time)
{
    d->lastModifed = time;
}

void SortFileInfo::setCreateTime(const qint64 time)
{
    d->create = time;
}

void SortFileInfo::setDisplayType(const QString &displayType)
{
    d->displayType = displayType;
}

void SortFileInfo::setHighlightContent(const QString &content)
{
    d->highlightContent = content;
}

void SortFileInfo::setDisplayName(const QString &displayName)
{
    d->displayName = displayName;
}

QString SortFileInfo::highlightContent() const
{
    return d->highlightContent;
}

QString SortFileInfo::displayName() const
{
    return d->displayName;
}

QUrl SortFileInfo::fileUrl() const
{
    return d->url;
}

qint64 SortFileInfo::fileSize() const
{
    return d->filesize;
}

bool SortFileInfo::isFile() const
{
    return d->file;
}

bool SortFileInfo::isDir() const
{
    return d->dir;
}

bool SortFileInfo::isSymLink() const
{
    return d->symLink;
}

bool SortFileInfo::isHide() const
{
    return d->hide;
}

bool SortFileInfo::isReadable() const
{
    return d->readable;
}

bool SortFileInfo::isWriteable() const
{
    return d->writeable;
}

bool SortFileInfo::isExecutable() const
{
    return d->executable;
}

QUrl SortFileInfo::symlinkTarget() const
{
    return d->symLinkTag;
}

qint64 SortFileInfo::lastReadTime() const
{
    return d->lastRead;
}

qint64 SortFileInfo::lastModifiedTime() const
{
    return d->lastModifed;
}

qint64 SortFileInfo::createTime() const
{
    return d->create;
}

QString SortFileInfo::displayType() const
{
    return d->displayType;
}

// 信息完整性相关方法
void SortFileInfo::setInfoCompleted(const bool completed)
{
    QMutexLocker locker(&d->mutex);
    d->infoCompleted = completed;
}

void SortFileInfo::markAsCompleted()
{
    setInfoCompleted(true);
}

bool SortFileInfo::isInfoCompleted() const
{
    QMutexLocker locker(&d->mutex);
    return d->infoCompleted;
}

bool SortFileInfo::needsCompletion() const
{
    return !isInfoCompleted();
}

// 新增：文件信息补全接口
bool SortFileInfo::completeFileInfo()
{
    if (isInfoCompleted()) {
        return true;  // 已经完成，无需重复获取
    }
    return d->doCompleteFileInfo();
}

SortFileInfoPrivate::SortFileInfoPrivate(SortFileInfo *qq)
    : q(qq)
{
}

SortFileInfoPrivate::~SortFileInfoPrivate()
{
}

bool SortFileInfoPrivate::doCompleteFileInfo()
{
    if (!url.isLocalFile()) {
        return false;
    }
    
    struct stat64 statBuffer;
    const QString filePath = url.path();
    
    if (::stat64(filePath.toUtf8().constData(), &statBuffer) != 0) {
        return false;
    }
    
    QMutexLocker locker(&mutex);
    
    // 一次性设置所有从 stat64 获取的信息
    
    // 基础信息
    filesize = statBuffer.st_size;
    file = S_ISREG(statBuffer.st_mode);
    dir = S_ISDIR(statBuffer.st_mode);
    auto symlPath = FileUtils::symlinkTarget(url);
    symLink = !symlPath.isEmpty();
    symLinkTag = symLink ? QUrl::fromLocalFile(symlPath) : QUrl();

    // 权限信息
    readable = statBuffer.st_mode & S_IRUSR;
    writeable = statBuffer.st_mode & S_IWUSR;
    executable = statBuffer.st_mode & S_IXUSR;

    // 时间戳信息
    lastModifed = statBuffer.st_mtime;

    // 隐藏文件检查
    QString fileName = url.fileName();
    hide = fileName.startsWith('.');

    // 标记所有信息已完成
    infoCompleted = true;

    return true;
}

void SortFileInfoPrivate::getDisplayName()
{
    if (!url.isValid())
        return;
    if (!url.isLocalFile() || FileUtils::isGvfsFile(url) || !url.toString().endsWith(".desktop")) {
        displayName = url.fileName();
    } else {
        try {
            // 注意：此处仍需确保 DesktopFile/Properties 完全线程安全
            DesktopFile desktopFile(url.path());
            if (desktopFile.desktopDeepinVendor() == QStringLiteral("deepin")
                    && !(desktopFile.desktopDisplayName().isEmpty())) {
                displayName = desktopFile.desktopDisplayName();
            } else {
                displayName = desktopFile.desktopLocalName().isEmpty() ? displayName
                                                                     : desktopFile.desktopLocalName();
            }
        } catch (const std::exception &e) {
            // 建议记录具体异常信息，而非吞掉所有异常
            qWarning() << "DesktopFile parse failed for" << url.path() << ":" << e.what();
            displayName = url.fileName();
        } catch (...) {
            qWarning() << "DesktopFile parse failed (unknown error) for" << url.path();
            displayName = url.fileName();
        }
    }
}

}
