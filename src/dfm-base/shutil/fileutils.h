/*
 * Copyright (C) 2016 ~ 2018 Deepin Technology Co., Ltd.
 *               2016 ~ 2018 dragondjf
 *
 * Author:     dragondjf<dingjiangfeng@deepin.com>
 *
 * Maintainer: dragondjf<dingjiangfeng@deepin.com>
 *             zccrs<zhangjide@deepin.com>
 *             Tangtong<tangtong@deepin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FILEUTILS_H
#define FILEUTILS_H

//#include "desktopfile.h"
#include "properties.h"
#include "base/dabstractfileinfo.h"
#include "utils/desktopinfo.h"
#include "dfm-base/dfm_base_global.h"

#include <QObject>
#include <QIcon>
#include <QFileInfo>
#include <QDir>
#include <QMimeType>
#include <QUrl>

class DFMLocalFileInfo;

/**
 * @class FileUtils
 * @brief Utility class providing static helper methods for file management
 */

class FileUtils
{
public:

    static QString XDG_RUNTIME_DIR;
    static QStringList CURRENT_ISGVFSFILE_PATH;

    static bool removeRecurse(const QString &path, const QString &name);
    static bool isAncestorUrl(const QUrl &ancestor, const QUrl &url);
    static void recurseFolder(const QString &path, const QString &parent,
                              QStringList *list);
    static int filesCount(const QString &dir);
    static QStringList filesList(const QString &dir);
    static qint64 singleDirSize(const QUrl &url);
    static qint64 totalSize(const QString &targetFile);
    static qint64 totalSize(const QList<QUrl> &files);
    //计算本地文件的大小
    static qint64 totalSize(const QList<QUrl> &files, qint32 &dirSize,qint32 &fileCount);
    static qint64 totalSize(const QList<QUrl> &files, const qint64 &maxLimit, bool &isInLimit);
    static bool isArchive(const QString &path);
    static bool canFastReadArchive(const QString &path);
    static QStringList getApplicationNames();
    static QString getRealSuffix(const QString &name);
    static QIcon searchGenericIcon(const QString &category,
                                   const QIcon &defaultIcon = QIcon::fromTheme("unknown"));
    static QIcon searchMimeIcon(QString mime,
                                const QIcon &defaultIcon = QIcon::fromTheme("unknown"));
    static QIcon searchAppIcon(const DFMLocalFileInfo &app,
                               const QIcon &defaultIcon = QIcon::fromTheme("application-x-executable"));
    static QString formatSize(qint64 num, bool withUnitVisible = true, int precision = 1, int forceUnit = -1, QStringList unitList = QStringList());
//    static QString diskUsageString(quint64 &usedSize, quint64 &totalSize, QString strVolTag = "");
//    static QString defaultOpticalSize(const QString &tagName, quint64 &usedSize, quint64 &totalSize);
    static QUrl newDocumentUrl(const DAbstractFileInfoPointer targetDirInfo, const QString &baseName, const QString &suffix);
    static QString newDocmentName(QString targetdir, const QString &baseName, const QString &suffix);
    static bool cpTemplateFileToTargetDir(const QString &targetdir, const QString &baseName, const QString &suffix, WId windowId);

    static bool openFile(const QString &filePath);
    static bool openFiles(const QStringList &filePaths);
    static bool openEnterFiles(const QStringList &filePaths);
    static bool launchApp(const QString &desktopFile, const QStringList &filePaths = {}); // open filePaths by desktopFile
    static bool launchAppByDBus(const QString &desktopFile, const QStringList &filePaths = {});
    static bool launchAppByGio(const QString &desktopFile, const QStringList &filePaths = {});

    static bool openFilesByApp(const QString &desktopFile, const QStringList &filePaths);
    static bool isFileManagerSelf(const QString &desktopFile);
    static QString defaultTerminalPath();

    static bool setBackground(const QString &pictureFilePath);

    static QString md5(const QString &fpath);
    static QByteArray md5(QFile *file, const QString &filePath);

    static bool isFileExecutable(const QString &path);
    static bool shouldAskUserToAddExecutableFlag(const QString &path);
    static bool isFileRunnable(const QString &path);
    static bool isFileWindowsUrlShortcut(const QString &path); /*check file is windows url shortcut*/
    static QString getInternetShortcutUrl(const QString &path);/*get InternetShortcut url of windows url shortcut*/

    static QString getFileMimetype(const QString &path);
    static bool isExecutableScript(const QString &path);
    static bool openExcutableScriptFile(const QString &path, int flag);
    static bool addExecutableFlagAndExecuse(const QString &path, int flag);
    static bool openExcutableFile(const QString &path, int flag);
    static bool runCommand(const QString &cmd, const QStringList &args, const QString &wd = QString());

    static void mkpath(const QUrl &path);
    static QString displayPath(const QString &pathStr);

    static QByteArray imageFormatName(QImage::Format f);

    static QString getFileContent(const QString &file);
    static bool writeTextFile(const QString &filePath, const QString &content);
    static void migrateConfigFileFromCache(const QString &key);
    static QMap<QString, QString> getKernelParameters();

//    static DFMGlobal::MenuExtension getMenuExtension(const QList<QUrl> &urlList);
//    static bool isGvfsMountFile(const QString &filePath, const bool &isEx = false);
    static bool isFileExists(const QString &filePath);

    static QJsonObject getJsonObjectFromFile(const QString &filePath);
    static QJsonArray getJsonArrayFromFile(const QString &filePath);
    static bool writeJsonObjectFile(const QString &filePath, const QJsonObject &obj);
    static bool writeJsonnArrayFile(const QString &filePath, const QJsonArray &array);

    static void mountAVFS();
    static void umountAVFS();

    static bool isDesktopFile(const QString &filePath);
    static bool isDesktopFile(const QFileInfo &fileInfo);
    static bool isDesktopFile(const QString &filePath, QMimeType &mimetype);
    static bool isDesktopFile(const QFileInfo &fileInfo, QMimeType &mimetyp);
//    static void addRecentFile(const QString &filePath, const DesktopFile &desktopFile, const QString &mimetype);

    static bool deviceShouldBeIgnore(const QString &devId); // devId = /dev/sdb(N)

    // 启用deepin-compressor追加压缩
//    static bool appendCompress(const QUrl &toUrl, const QList<QUrl> &fromUrlList);
    //获取内存叶大小
    static int getMemoryPageSize();
    //获取线程cpu核个数
    static qint32 getCpuProcessCount();
    //判断是否是操作系统同盘的本地文件
    static bool isFileOnDisk(const QString &path);
};

#endif // FILEUTILS_H
