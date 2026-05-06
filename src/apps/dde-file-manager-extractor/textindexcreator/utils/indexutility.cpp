// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "indexutility.h"
#include "textindexconfig.h"

#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDir>
#include <QStandardPaths>
#include <QSaveFile>

inline constexpr char kDeepinAnythingDconfName[] { "org.deepin.anything" };
inline constexpr char kDeepinAnythingDconfPathKey[] { "indexing_paths" };
inline constexpr char kDeepinAnythingDconfBlacklistPathKey[] { "blacklist_paths" };

DCORE_USE_NAMESPACE
TEXTINDEX_CREATOR_BEGIN_NAMESPACE

namespace IndexUtility {

bool isIndexWithAnything(const QString &path)
{
    auto status = DFMSEARCH::Global::fileNameIndexStatus();
    if (!status.has_value()) {
        fmWarning() << "Anything indexing is disabled";
        return {};
    }

    const QString currentStatus = status.value();
    if (currentStatus == "closed") {
        fmWarning() << "Anything indexing is closed";
        return {};
    }

    return isDefaultIndexedDirectory(path);
}

bool isDefaultIndexedDirectory(const QString &path)
{
    auto kDirs = AnythingConfigWatcher::instance()->defaultAnythingIndexPaths();
    return kDirs.contains(path);
}

bool checkFileSize(const QFileInfo &fileInfo, qint64 sizeMBFromConfig)
{
    try {
        // 在这里进行上述的健全性检查
        if (sizeMBFromConfig <= 0 || sizeMBFromConfig > Q_INT64_C(0x7FFFFFFFFFFFFFFF) / (1024LL * 1024LL)) {
            sizeMBFromConfig = 50LL;   // Default fallback
        }
        const qint64 kMaxFileSizeInBytes = sizeMBFromConfig * 1024LL * 1024LL;

        if (fileInfo.size() > kMaxFileSizeInBytes) {
            fmDebug() << "File" << fileInfo.fileName() << "size" << fileInfo.size()
                      << "exceeds max allowed size" << kMaxFileSizeInBytes;
            return false;
        }
        return true;
    } catch (const std::exception &e) {
        fmWarning() << "Failed to check file size:" << fileInfo.filePath() << e.what();
        return false;
    } catch (...) {
        fmWarning() << "Failed to check file size with unknown exception:" << fileInfo.filePath();
        return false;
    }
}

bool isSupportedTextFile(const QString &path)
{
    try {
        QFileInfo fileInfo(path);

        // 检查文件大小是否超过 X MB（X * 1024 * 1024 字节）
        if (fileInfo.exists() && !checkFileSize(fileInfo, TextIndexConfig::instance().maxIndexTextFileSizeMB()))
            return false;

        const QString &suffix = fileInfo.suffix().toLower();
        return TextIndexConfig::instance().supportedTextFileExtensions().contains(suffix);
    } catch (const std::exception &e) {
        fmWarning() << "Failed to check if file is supported:" << path << e.what();
        return false;
    } catch (...) {
        fmWarning() << "Failed to check if file is supported with unknown exception:" << path;
        return false;
    }
}

AnythingConfigWatcher *AnythingConfigWatcher::instance()
{
    static AnythingConfigWatcher *in = new AnythingConfigWatcher;
    return in;
}

AnythingConfigWatcher::~AnythingConfigWatcher()
{
}

QStringList AnythingConfigWatcher::defaultAnythingIndexPaths()
{
    QMutexLocker lk(&mu);
    return defaultIndexPath;
}

QStringList AnythingConfigWatcher::defaultAnythingIndexPathsRealtime()
{
    QMutexLocker lk(&mu);
    defaultIndexPath.clear();
    defaultIndexPath = DFMSEARCH::Global::defaultIndexedDirectory();
    return defaultIndexPath;
}

QStringList AnythingConfigWatcher::defaultBlacklistPaths()
{
    QMutexLocker lk(&mu);
    return blacklistPaths;
}

QStringList AnythingConfigWatcher::defaultBlacklistPathsRealtime()
{
    QMutexLocker lk(&mu);
    blacklistPaths.clear();
    blacklistPaths = IndexUtility::defaultBlacklistPaths();
    return blacklistPaths;
}

void AnythingConfigWatcher::handleConfigChanged(const QString &key)
{
    if (key == kDeepinAnythingDconfPathKey) {
    defaultAnythingIndexPathsRealtime();
        fmInfo() << "Anything indexing paths changed, index rebuild needed";
        emit rebuildRequired(QStringLiteral("anythingIndexPathsChanged"));
    } else if (key == kDeepinAnythingDconfBlacklistPathKey) {
        defaultBlacklistPathsRealtime();
        fmInfo() << "Anything blacklist paths changed, index rebuild needed";
        emit rebuildRequired(QStringLiteral("anythingBlacklistPathsChanged"));
    }
}

AnythingConfigWatcher::AnythingConfigWatcher(QObject *parent)
    : QObject(parent)
{
    cfg = DConfig::create(kDeepinAnythingDconfName, kDeepinAnythingDconfName, "", this);
    if (!cfg)
        qWarning() << " [AnythingConfigWatcher::AnythingConfigWatcher] create dconfig error, nullptr!!" << kDeepinAnythingDconfName;

    if (cfg && !cfg->isValid()) {
        qWarning() << " [AnythingConfigWatcher::AnythingConfigWatcher] create dconfig error, config is not valid!!" << kDeepinAnythingDconfName;
        cfg->deleteLater();
        cfg = nullptr;
    }

    if (cfg && cfg->isValid())
        connect(cfg, &DConfig::valueChanged, this, &AnythingConfigWatcher::handleConfigChanged);

    defaultAnythingIndexPathsRealtime();
    defaultBlacklistPathsRealtime();
}

// This function is internal to this unit (static) and handles the core DConfig loading.
static std::optional<QStringList> tryLoadStringListFromDConfigInternal(
        const QString &appId,
        const QString &schemaId,
        const QString &keyName)
{
    // Dtk::Core::DConfig::create returns a pointer and ideally needs a parent
    // for memory management. For a short-lived object within a function,
    // a local QObject can serve as a temporary parent.
    QObject dconfigParent;   // Temporary parent for the DConfig instance
    Dtk::Core::DConfig *dconfigPtr = Dtk::Core::DConfig::create(appId, schemaId, "", &dconfigParent);

    // Check if DConfig object was created successfully
    if (!dconfigPtr) {
        qWarning() << "DConfig: Failed to create DConfig instance for appId:" << appId << "schemaId:" << schemaId;
        return std::nullopt;
    }

    // Check if the created DConfig instance is valid
    if (!dconfigPtr->isValid()) {
        qWarning() << "DConfig: Instance is invalid for appId:" << appId << "schemaId:" << schemaId;
        // No need to delete dconfigPtr, dconfigParent will manage it.
        return std::nullopt;
    }

    QVariant value = dconfigPtr->value(keyName);

    if (!value.isValid()) {
        qDebug() << "DConfig: Key '" << keyName << "' not found in appId:" << appId << "schemaId:" << schemaId;
        return std::nullopt;   // Key not found
    }

    if (!value.canConvert<QStringList>()) {
        qWarning() << "DConfig: Value for key '" << keyName << "' in appId:" << appId << "schemaId:" << schemaId
                   << "cannot be converted to QStringList. Actual type:" << value.typeName();
        return std::nullopt;   // Type mismatch
    }
    // No need to delete dconfigPtr, dconfigParent will manage it when it goes out of scope.
    return value.toStringList();
}

// --- Specific Loader for "blacklist_paths" ---
static std::optional<QStringList> tryLoadBlacklistPathsFromDConfig()
{
    const QString appId = "org.deepin.anything";
    const QString schemaId = "org.deepin.anything";
    const QString keyName = "blacklist_paths";

    std::optional<QStringList> stringListOpt = tryLoadStringListFromDConfigInternal(appId, schemaId, keyName);

    if (!stringListOpt) {
        return std::nullopt;   // Loading failed
    }

    const QStringList &pathsFromDConfigList = *stringListOpt;

    if (pathsFromDConfigList.isEmpty()) {
        qDebug() << "DConfig: Key '" << keyName << "' in schema '" << schemaId << "' provided an empty list.";
    }
    return pathsFromDConfigList;   // Return the processed list
}

QStringList defaultBlacklistPaths()
{
    std::optional<QStringList> dconfigPathsOpt = tryLoadBlacklistPathsFromDConfig();

    if (!dconfigPathsOpt) {
        qDebug() << "Failed to load blacklist paths from DConfig or DConfig instance invalid, returning empty list.";
        return QStringList();
    }

    const QStringList &pathsFromDConfig = *dconfigPathsOpt;
    qDebug() << "Resolved blacklist paths:" << pathsFromDConfig;
    return pathsFromDConfig;
}

}   // namespace IndexUtility

namespace PathCalculator {

QString calculateNewPathForDirectoryMove(const QString &oldPath,
                                         const QString &fromDirPath,
                                         const QString &toDirPath)
{
    if (oldPath.startsWith(fromDirPath)) {
        return toDirPath + "/" + oldPath.mid(fromDirPath.length());
    } else if (oldPath == fromDirPath.chopped(1)) {   // Remove trailing slash for comparison
        return toDirPath;
    }
    return oldPath;   // No change needed
}

QString normalizeDirectoryPath(const QString &dirPath)
{
    QString normalized = dirPath;
    if (!normalized.endsWith('/')) {
        normalized += '/';
    }
    return normalized;
}

bool isDirectoryMove(const QString &toPath)
{
    if (toPath.isEmpty()) {
        return false;
    }

    // First check if path exists and is a directory
    QFileInfo toFileInfo(toPath);
    if (toFileInfo.exists()) {
        return toFileInfo.isDir();
    }

    // If path doesn't exist, infer from path format (trailing slash indicates directory)
    return toPath.endsWith('/');
}

QStringList extractAncestorPaths(const QString &filePath)
{
    QStringList ancestorPaths;
    if (filePath.isEmpty())
        return ancestorPaths;

    QFileInfo fileInfo(filePath);
    QString currentPath = fileInfo.path();   // 初始为文件所在的目录路径

    // 循环向上获取所有父目录，直到根目录
    while (currentPath != "/" && !currentPath.isEmpty()) {
        ancestorPaths.append(currentPath);
        currentPath = QFileInfo(currentPath).path();   // 获取父目录
    }

    // 如果文件路径是根目录下的文件，确保包括根目录
    if (fileInfo.path() == "/" && fileInfo.exists() && fileInfo.isFile()) {
        // 文件直接在根目录下，没有祖先目录
        return ancestorPaths;
    }

    // 添加根目录如果当前路径是 "/" 但还没有添加（处理目录路径的情况）
    if (currentPath == "/" && !ancestorPaths.contains("/")) {
        // 这种情况应该不会发生，因为文件路径是文件不是目录，fileInfo.path() 只会在文件直接在根目录下时返回 "/"
        // 而上面的条件已经处理了这种情况
    }

    return ancestorPaths;
}

}   // namespace PathCalculator

TEXTINDEX_CREATOR_END_NAMESPACE
