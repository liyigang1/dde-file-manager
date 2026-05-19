// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktopfile.h"
#include "properties.h"

#include <QFile>
#include <QLocale>
#include <QDebug>

using namespace dfmbase;

DesktopFile::DesktopFile(const QString &fileName)
    : fileName(fileName)
{
    // File validity
    if (fileName.isEmpty() || !QFile::exists(fileName)) {
        return;
    }

    // 使用 Properties 读取 .desktop 文件，避免 QSettings 的线程安全问题
    // (QSettings 内部访问 QCoreApplication 全局状态，非线程安全)
    Properties desktop(fileName, "Desktop Entry");

    if (desktop.contains("X-Deepin-AppID")) {
        deepinId = desktop.value("X-Deepin-AppID").toString();
    }

    if (desktop.contains("X-Deepin-Vendor")) {
        deepinVendor = desktop.value("X-Deepin-Vendor").toString();
    }

    if (desktop.contains("NoDisplay")) {
        noDisplay = desktop.value("NoDisplay").toBool();
    }
    if (desktop.contains("Hidden")) {
        hidden = desktop.value("Hidden").toBool();
    }

    // 缓存系统语言名，避免多线程并发调用 QLocale::system()
    static const QString cachedLocaleName = QLocale::system().name();

    auto getValueFromSys = [&desktop](const QString &type, const QString &sysName) -> QString {
        const QString key = QString("%0[%1]").arg(type).arg(sysName);
        return desktop.value(key).toString();
    };

    auto getNameByType = [&desktop, &getValueFromSys](const QString &type) -> QString {
        QString targetName = getValueFromSys(type, cachedLocaleName);
        if (targetName.isEmpty()) {
            auto strSize = cachedLocaleName.trimmed().split("_");
            if (!strSize.isEmpty()) {
                targetName = getValueFromSys(type, strSize.first());
            }

            if (targetName.isEmpty())
                targetName = desktop.value(type).toString();
        }

        return targetName;
    };
    localName = getNameByType("Name");
    genericName = getNameByType("GenericName");

    exec = desktop.value("Exec").toString();
    icon = desktop.value("Icon").toString();
    type = desktop.value("Type", "Application").toString();
    categories = desktop.value("Categories").toString().remove(" ").split(";");

    QString mimeTypeTemp = desktop.value("MimeType").toString().remove(" ");

    if (!mimeTypeTemp.isEmpty())
        mimeType = mimeTypeTemp.split(";");
    // Fix categories
    if (categories.first().compare("") == 0) {
        categories.removeFirst();
    }
}
//---------------------------------------------------------------------------

QString DesktopFile::desktopFileName() const
{
    return fileName;
}
//---------------------------------------------------------------------------

QString DesktopFile::desktopPureFileName() const
{
    return fileName.split("/").last().remove(".desktop");
}
//---------------------------------------------------------------------------

QString DesktopFile::desktopName() const
{
    return name;
}

QString DesktopFile::desktopLocalName() const
{
    return localName;
}

QString DesktopFile::desktopDisplayName() const
{
    if (deepinVendor == QStringLiteral("deepin") && !genericName.isEmpty()) {
        return genericName;
    }
    return localName.isEmpty() ? name : localName;
}
//---------------------------------------------------------------------------

QString DesktopFile::desktopExec() const
{
    return exec;
}
//---------------------------------------------------------------------------

QString DesktopFile::desktopIcon() const
{
    return icon;
}
//---------------------------------------------------------------------------

QString DesktopFile::desktopType() const
{
    return type;
}

QString DesktopFile::desktopDeepinId() const
{
    return deepinId;
}

QString DesktopFile::desktopDeepinVendor() const
{
    return deepinVendor;
}

bool DesktopFile::isNoShow() const
{
    if (hidden)
        return true;
    // task: 369003
    if (noDisplay && mimeType.isEmpty())
        return true;
    return false;
}

//---------------------------------------------------------------------------

QStringList DesktopFile::desktopCategories() const
{
    return categories;
}
//---------------------------------------------------------------------------

QStringList DesktopFile::desktopMimeType() const
{
    return mimeType;
}
//---------------------------------------------------------------------------
