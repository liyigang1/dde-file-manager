// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktopfileinfo.h"
#include <dfm-base/utils/desktopfile.h>
#include <dfm-base/utils/properties.h>
#include <dfm-base/utils/fileutils.h>
#include <dfm-base/utils/chinese2pinyin.h>
#include <dfm-base/base/schemefactory.h>

#include <QDir>
#include <QSettings>
#include <QLocale>
#include <QProcess>
#include <QStandardPaths>
#include <QAtomicInteger>

using namespace dfmbase;

namespace dfmbase {
class DesktopFileInfoPrivate : public QSharedData
{
public:
    inline explicit DesktopFileInfoPrivate(const QUrl &url)
        : QSharedData()
    {
        updateInfo(url);
    }

    inline DesktopFileInfoPrivate(const DesktopFileInfoPrivate &copy)
        : QSharedData(copy)
    {
    }

    inline ~DesktopFileInfoPrivate()
    {
    }

    void updateInfo(const QUrl &fileUrl)
    {
        const QMap<QString, QVariant> &map = DesktopFileInfo::desktopFileInfo(fileUrl);

        name = map.value("Name").toString();
        genericName = map.value("GenericName").toString();
        exec = map.value("Exec").toString();
        iconName = map.value("Icon").toString();
        type = map.value("Type").toString();
        categories = map.value("Categories").toStringList();
        mimeType = map.value("MimeType").toStringList();
        deepinID = map.value("DeepinID").toString();
        deepinVendor = map.value("DeepinVendor").toString();
        // Fix categories
        if (!categories.isEmpty() && categories.first().compare("") == 0) {
            categories.removeFirst();
        }

        if (iconName == "user-trash") {
            if (!FileUtils::trashIsEmpty())
                iconName = "user-trash-full";
        }

        if (!iconName.isEmpty() && QIcon::hasThemeIcon(iconName))
            hasThemeIcon.storeRelease(true);

        icon = QIcon();
    }

    QIcon findIcon(const QString &iconName);

public:
    QString name;
    QString genericName;
    QString exec;
    QIcon icon;
    QString iconName;
    QString type;
    QStringList categories;
    QStringList mimeType;
    QString deepinID;
    QString deepinVendor;
    QAtomicInteger<bool> hasThemeIcon { false };
    QAtomicInteger<bool> useProxyIcon { false };
};
}

DesktopFileInfo::DesktopFileInfo(const QUrl &fileUrl)
    : DesktopFileInfo(fileUrl, InfoFactory::create<FileInfo>(fileUrl))
{
}

DesktopFileInfo::DesktopFileInfo(const QUrl &fileUrl, const FileInfoPointer &info)
    : ProxyFileInfo(fileUrl), d(new DesktopFileInfoPrivate(fileUrl))
{
    setProxy(info);
}

DesktopFileInfo::~DesktopFileInfo()
{
}

QString DesktopFileInfo::desktopName() const
{
    if (d->deepinVendor == QStringLiteral("deepin") && !(d->genericName.isEmpty())) {
        return d->genericName;
    }

    return d->name;
}

QString DesktopFileInfo::desktopExec() const
{
    return d->exec;
}

QString DesktopFileInfo::desktopIconName() const
{
    if (d->hasThemeIcon.loadAcquire())
        return d->iconName;

    return "desktopNotThemeIcon::"+d->iconName;
}

QString DesktopFileInfo::desktopType() const
{
    return d->type;
}

QStringList DesktopFileInfo::desktopCategories() const
{
    return d->categories;
}

QIcon DesktopFileInfo::fileIcon()
{
    if (Q_LIKELY(!d->icon.isNull()))
        return d->icon;

    if (d->useProxyIcon)
        return proxy->fileIcon();

    const QString iconName = this->nameOf(NameInfoType::kIconName).replace("desktopNotThemeIcon::", "");

    if (iconName.startsWith("data:image/")) {
        int firstSemicolon = iconName.indexOf(';', 11);

        if (firstSemicolon > 11) {
            // iconPath is a string representing an inline image.

            int base64strPos = iconName.indexOf("base64,", firstSemicolon);

            if (base64strPos > 0) {
                QPixmap pixmap;

                bool ok = pixmap.loadFromData(QByteArray::fromBase64(iconName.mid(base64strPos + 7).toLatin1()) /*, format.toLatin1().constData()*/);

                if (ok) {
                    d->icon = QIcon(pixmap);
                } else {
                    d->icon = QIcon::fromTheme("application-default-icon");
                }
            }
        }
    } else {
        const QString &currentDir = QDir::currentPath();

        QDir::setCurrent(pathOf(PathInfoType::kAbsolutePath));

        QFileInfo fileInfo(iconName.startsWith("~") ? (QDir::homePath() + iconName.mid(1)) : iconName);

        if (!fileInfo.exists())
            fileInfo.setFile(QUrl::fromUserInput(iconName).toLocalFile());

        if (fileInfo.exists()) {
            d->icon = QIcon(fileInfo.absoluteFilePath());
        }

        QDir::setCurrent(currentDir);

        if (!d->icon.isNull() && QPixmap(fileInfo.absoluteFilePath()).isNull())
            d->icon = QIcon();
    }

    if (d->icon.isNull() && !iconName.isEmpty()) {
        d->icon = QIcon::fromTheme(iconName);
        // https://bugreports.qt.io/browse/QTBUG-112257
        // Try to update icon cache if icon not found
        if (d->icon.isNull()) {
            qCDebug(logDFMBase) << "update theme cache for desktop file:" << urlOf(UrlInfoType::kUrl);
            QIcon::setThemeSearchPaths(QIcon::themeSearchPaths());
            d->icon = QIcon::fromTheme(iconName);

            // If still null, try to find icon manually
            if (d->icon.isNull()) {
                d->icon = d->findIcon(iconName);
                qCWarning(logDFMBase) << "findIcon result:" << (d->icon.isNull() ? "null" : "found") << iconName;
            }
        }
    }

    if (d->icon.isNull()) {
        d->useProxyIcon.storeRelease(true);
        return ProxyFileInfo::fileIcon();
    }

    return d->icon;
}

QString DesktopFileInfo::nameOf(const NameInfoType type) const
{
    switch (type) {
    case NameInfoType::kFileNameOfRename:
        [[fallthrough]];
    case NameInfoType::kBaseNameOfRename:
        return displayOf(DisPlayInfoType::kFileDisplayName);
    case NameInfoType::kSuffixOfRename:
        return QString();
    case NameInfoType::kFileCopyName:
        return ProxyFileInfo::nameOf(NameInfoType::kFileName);
    case NameInfoType::kIconName:
        return desktopIconName();
    case NameInfoType::kGenericIconName:
        return !d->genericName.isEmpty() && QIcon::hasThemeIcon(d->genericName)
                ? d->genericName : QStringLiteral("application-default-icon");
    default:
        return ProxyFileInfo::nameOf(type);
    }
}

QString DesktopFileInfo::displayOf(const DisPlayInfoType type) const
{
    const auto &&name = desktopName();
    if (type == DisPlayInfoType::kFileDisplayName && !name.isEmpty())
        return name;

    if (type == DisPlayInfoType::kFileDisplayPinyinName && !name.isEmpty())
        return Pinyin::Chinese2Pinyin(name);

    return ProxyFileInfo::displayOf(type);
}

void DesktopFileInfo::refresh()
{
    ProxyFileInfo::refresh();
    d->updateInfo(urlOf(UrlInfoType::kUrl));
}

Qt::DropActions DesktopFileInfo::supportedOfAttributes(const SupportType type) const
{
    if (type == SupportType::kDrag && (d->deepinID == "dde-trash" || d->deepinID == "dde-computer")) {
        return Qt::IgnoreAction;
    }

    return ProxyFileInfo::supportedOfAttributes(type);
}

void DesktopFileInfo::updateAttributes(const QList<FileInfo::FileInfoAttributeID> &types)
{
    ProxyFileInfo::updateAttributes(types);
    d->updateInfo(urlOf(UrlInfoType::kUrl));
}

bool DesktopFileInfo::canTag() const
{
    if (d->deepinID == "dde-trash" || d->deepinID == "dde-computer")
        return false;

    //桌面主目录不支持添加tag功能
    if (d->deepinID == "dde-file-manager" && d->exec.contains(" -O "))
        return false;

    return true;
}

bool DesktopFileInfo::canAttributes(const CanableInfoType type) const
{
    switch (type) {
    case FileCanType::kCanMoveOrCopy:
        //部分桌面文件不允许复制或剪切
        if (d->deepinID == "dde-trash" || d->deepinID == "dde-computer")
            return false;

        //exec执行字符串中“-O”参数表示打开主目录
        if (d->deepinID == "dde-file-manager" && d->exec.contains(" -O "))
            return false;

        return true;
    case FileCanType::kCanDrop:
        if (d->deepinID == "dde-computer")
            return false;

        return ProxyFileInfo::canAttributes(type);
    default:
        return ProxyFileInfo::canAttributes(type);
    }
}

QMap<QString, QVariant> DesktopFileInfo::desktopFileInfo(const QUrl &fileUrl)
{
    QMap<QString, QVariant> map;
    DesktopFile desktopFile(fileUrl.path());

    map["Name"] = desktopFile.desktopLocalName();
    map["GenericName"] = desktopFile.desktopDisplayName();

    map["Exec"] = desktopFile.desktopExec();
    map["Icon"] = desktopFile.desktopIcon();
    map["Type"] = desktopFile.desktopType();
    map["Categories"] = desktopFile.desktopCategories();
    map["MimeType"] = desktopFile.desktopMimeType();
    map["DeepinID"] = desktopFile.desktopDeepinId();
    map["DeepinVendor"] = desktopFile.desktopDeepinVendor();

    return map;
}

QIcon DesktopFileInfoPrivate::findIcon(const QString &iconName)
{
    if (iconName.isEmpty())
        return QIcon();

    // Build icon search paths following XDG Icon Theme Specification
    // instead of relying on QIcon::themeSearchPaths() which may miss newly installed directories
    static const QStringList kIconDirs = []() {
        QStringList dirs;
        const QString home = QDir::homePath();
        // XDG_DATA_DIRS/icons (default: /usr/local/share/icons, /usr/share/icons)
        // highest priority - system and newly installed icon packages land here
        const QString dataDirs = qEnvironmentVariable("XDG_DATA_DIRS",
                                                       "/usr/local/share:/usr/share");
        for (const QString &d : dataDirs.split(':', QString::SkipEmptyParts))
            dirs << d + "/icons";
        // XDG_DATA_HOME/icons (default: ~/.local/share/icons)
        const QString dataHome = qEnvironmentVariable("XDG_DATA_HOME",
                                                       home + "/.local/share");
        dirs << dataHome + "/icons";
        // ~/.icons
        dirs << home + "/.icons";
        // /usr/share/pixmaps (legacy XDG path)
        dirs << "/usr/share/pixmaps";
        dirs.removeDuplicates();
        return dirs;
    }();
    const QStringList &searchPaths = kIconDirs;
    for (const QString &basePath : searchPaths) {
        if (!QDir(basePath).exists())
            continue;

        QProcess process;
        process.start("find", { basePath, "-name", QString("*%1*").arg(iconName) });
        if (!process.waitForFinished(3000)) {
            qCWarning(logDFMBase) << "find process timeout for" << iconName << "in" << basePath;
            process.kill();
            continue;
        }

        if (process.exitCode() != 0)
            continue;

        const QString &error = process.readAllStandardError();
        if (!error.isEmpty())
            qCWarning(logDFMBase) << "find error:" << error;

        const QString &output = process.readAllStandardOutput().trimmed();
        if (output.isEmpty())
            continue;

        static const QStringList kIconSuffixes = { ".png", ".svg", ".xpm" };
        static const QStringList kPriorityPatterns = { "512x512", "256x256", "scalable", "64x64", "48x48" };

        QString bestPath;
        for (const QString &line : output.split('\n', QString::SkipEmptyParts)) {
            const QString &path = line.trimmed();
            if (path.isEmpty())
                continue;

            const bool validSuffix = std::any_of(kIconSuffixes.cbegin(), kIconSuffixes.cend(),
                                                  [&path](const QString &s) { return path.endsWith(s, Qt::CaseInsensitive); });
            if (!validSuffix)
                continue;

            const bool isPriority = std::any_of(kPriorityPatterns.cbegin(), kPriorityPatterns.cend(),
                                                [&path](const QString &p) { return path.contains(p); });
            if (isPriority) {
                bestPath = path;
                break;
            }

            if (bestPath.isEmpty())
                bestPath = path;
        }

        if (!bestPath.isEmpty())
            return QIcon(bestPath);
    }
    return QIcon();
}
