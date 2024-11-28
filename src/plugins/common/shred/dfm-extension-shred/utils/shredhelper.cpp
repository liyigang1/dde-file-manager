// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "shredhelper.h"

#include "../dialogs/filelistdialog.h"
#include "../dialogs/progressdialog.h"

#include <dfm-base/base/configs/dconfig/dconfigmanager.h>
#include <dfm-base/dfm_log_defines.h>

#include <QProcess>
#include <QProgressDialog>
#include <QApplication>
#include <QMessageBox>
#include <QRegExp>
#include <QFileInfo>
#include <QDir>
#include <QTranslator>
#include <QFile>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QDebug>

#include <iostream>
#include <sys/stat.h>
#include <linux/limits.h>

using namespace dfmbase;

namespace dfm_extenison_shred {
DFM_LOG_REISGER_CATEGORY(dfm_extenison_shred)
DFM_LOG_USE_CATEGORY(dfm_extenison_shred)

const QString kShredDConfigName = "org.deepin.dde.file-manager.shred";

bool ShredHelper::isShredEnabled()
{
    const QVariant vRe = DConfigManager::instance()->value(kShredDConfigName, "shred.enabled");
    return vRe.toBool();
}

void ShredHelper::initDconfig()
{
    QString err;
    if (!DConfigManager::instance()->addConfig(kShredDConfigName, &err))
        fmWarning() << "Shred: create dconfig failed: " << err;
}

bool ShredHelper::isValidPath(const std::string &path)
{
    // 获取用户主目录
    const char *homeDir = getenv("HOME");
    if (!homeDir)
        return false;   // 如果无法获取用户目录，为安全起见返回 false

    std::string homePath(homeDir);
    std::string realPath;

    // 获取真实路径(解析软链接)
    char resolvedPath[PATH_MAX];
    if (realpath(path.c_str(), resolvedPath) != nullptr) {
        realPath = resolvedPath;
    } else {
        realPath = path;   // 如果无法解析，使用原始路径
    }

    // 检查是否为U盘路径（以/media/开头）
    if (realPath.compare(0, 7, "/media/") == 0) {
        QStorageInfo storage(QString::fromStdString(realPath));
        // 检查设备是否是可移动设备
        if (storage.isValid() && storage.isReady() && storage.device().startsWith("/dev/sd")) {
            return true;
        }
        return false;
    }

    // 如果是数据盘路径，移除/data前缀后再判断
    if (realPath.compare(0, 6, "/data/") == 0) {
        realPath = realPath.substr(5);   // 移除"/data"（保留后面的斜杠）
    }

    // 检查是否为用户主目录下的路径
    if (realPath.length() <= homePath.length()
        || realPath.compare(0, homePath.length(), homePath) != 0
        || realPath == homePath) {
        //std::cout << "Path must be inside user's home directory or media directory. Current path: " << realPath << std::endl;
        return false;
    }

    // 检查是否为特殊目录
    const std::vector<std::string> protectedDirs = {
        homePath + "/Desktop",
        homePath + "/Downloads",
        homePath + "/Documents",
        homePath + "/Pictures",
        homePath + "/Music",
        homePath + "/Videos",
        homePath + "/Templates",
        homePath + "/Public"
    };

    // 检查路径是否为受保护目录（只检查目录本身，不包括子目录）
    for (const auto &dir : protectedDirs) {
        if (realPath == dir) {
            fmWarning() << "Cannot shred protected directory: " << dir.data();
            return false;
        }
    }

    return true;
}

ShredHelper::ShredHelper(QObject *parent)
    : QObject(parent), m_progressDialog(nullptr)
{
    auto translator = new QTranslator(this);
    translator->load(QLocale(), "shred-menu", "_", "/usr/share/dde-file-manager/translations/shred/menu");
    QCoreApplication::installTranslator(translator);
}

ShredHelper::~ShredHelper()
{
    if (m_progressDialog) {
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

QString ShredHelper::actionName()
{
    return tr("File Shred");
}

void ShredHelper::initProgressDialog()
{
    if (!m_progressDialog) {
        m_progressDialog = new ProgressDialog();

        // 初始化计数器
        m_totalFiles = 0;
        m_processedFiles = 0;
    }
}

void ShredHelper::shredfile(const std::vector<std::string> &filePaths)
{
    if (filePaths.empty())
        return;

    // 将 std::string 路径转换为 QStringList
    QStringList qFilePaths;
    for (const auto &path : filePaths) {
        qFilePaths << QString::fromStdString(path);
    }

    FileListDialog fileDlg(qFilePaths);
    int result = fileDlg.exec();
    if (result != 1)
        return;

    initProgressDialog();

    // 计算所有文件的总数
    m_totalFiles = 0;
    m_processedFiles = 0;

    QStringList regularFiles;
    QStringList directories;

    // 首先统计文件数并分类
    for (const auto &path : filePaths) {
        QString qPath = QString::fromStdString(path);
        QFileInfo fileInfo(qPath);

        if (isPipe(qPath)) {
            QFile::remove(qPath);
            continue;   // 管道文件直接删除并跳过后续处理
        }

        if (fileInfo.isSymLink()) {
            QFile::remove(qPath);
        } else if (fileInfo.isDir()) {
            directories << qPath;
            m_totalFiles += countFilesInDirectory(qPath);
        } else {
            regularFiles << qPath;
            m_totalFiles++;
        }
    }

    fmDebug() << "Total files to process: " << m_totalFiles;

    m_progressDialog->show();

    // 先处理普通文件
    m_progressDialog->updateProgressValue(0, "");
    if (!executeShredCommandBatch(regularFiles))
        return;

    // 再处理目录
    for (const QString &dir : directories) {
        processDirectory(dir);
    }
}

void ShredHelper::shredfile(const std::string &filePath)
{
    std::vector<std::string> paths = { filePath };
    shredfile(paths);
}

int ShredHelper::countFilesInDirectory(const QString &dirPath)
{
    int count = 0;
    QDir dir(dirPath);

    // 修改：使用 QDir::Hidden 标志来包含隐藏文件和目录
    QFileInfoList entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden | QDir::System);

    for (const QFileInfo &entry : entries) {
        if (entry.isSymLink()) {
            count++;
        } else if (entry.isDir()) {
            count += countFilesInDirectory(entry.absoluteFilePath());
            count++;
        } else {
            count++;
        }
    }

    //    std::cout << "Directory " << dirPath.toStdString()
    //              << " contains " << count << " items (including hidden files and subdirectories)"
    //              << std::endl;

    return count;
}

void ShredHelper::processDirectory(const QString &dirPath)
{
    QDir dir(dirPath);
    // 修改：使用 QDir::Hidden 和 QDir::System 标志来包含隐藏文件和系统文件
    QFileInfoList entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden | QDir::System);

    // 收集普通文件和隐藏文件的路径
    QStringList filesToShred;
    QStringList dirsToProcess;

    for (const QFileInfo &entry : entries) {
        if (isPipe(entry.absoluteFilePath())) {
            QFile::remove(entry.absoluteFilePath());
            continue;   // 管道文件直接删除并跳过后续处理
        }

        if (entry.isSymLink()) {
            QFile::remove(entry.absoluteFilePath());
        } else if (entry.isDir()) {
            dirsToProcess << entry.absoluteFilePath();
        } else {
            filesToShred << entry.absoluteFilePath();
        }
    }

    // 先批量处理文件
    if (!filesToShred.isEmpty()) {
        if (!executeShredCommandBatch(filesToShred))
            return;
    }

    // 递归处理子目录
    for (const QString &subDir : dirsToProcess) {
        processDirectory(subDir);
    }

    // 最后删除当前目录
    dir.rmdir(dirPath);
}

bool ShredHelper::executeShredCommandBatch(const QStringList &filePaths)
{
    if (filePaths.isEmpty())
        return true;

    QStringList args;
    args << "-u"
         << "-f"
         << "-v"
         << "-n"
         << "3";
    args.append(filePaths);

    QProcess process;

    // 设置固定的中文环境
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("LANG", "zh_CN.UTF-8");
    env.insert("LC_ALL", "zh_CN.UTF-8");
    env.insert("LANGUAGE", "zh_CN:zh");
    process.setProcessEnvironment(env);

    process.start("shred", args);

    if (!process.waitForStarted()) {
        fmWarning() << "Failed to start shred command for batch operation";
        return false;
    }

    QString savedErrorMsg;   // 用于保存错误信息

    while (process.state() != QProcess::NotRunning) {
        process.waitForReadyRead(100);
        QString output = QString::fromLocal8Bit(process.readAllStandardError());

        // 只在还没有保存错误信息时才提取文件路径
        if (savedErrorMsg.isEmpty() && output.contains("无法以写模式打开")) {
            QRegExp rx("shred:\\s*([^：]+)");   // 只匹配冒号前的文件名/路径部分
            if (rx.indexIn(output) != -1) {
                savedErrorMsg = rx.cap(1).trimmed();   // 只保存第一次遇到的文件名/路径
            }
        }

        updateProgress(output);
        QApplication::processEvents();
    }

    if (process.exitCode() != 0) {
        QString errorMsg;
        errorMsg = savedErrorMsg + ": " + tr("Permission denied");

        if (savedErrorMsg.isEmpty())
            errorMsg = tr("The file has been moved or the process has exited");
        m_progressDialog->handleShredResult(false, errorMsg);
        m_progressDialog->exec();
        fmWarning() << "Batch shred command failed: " << process.exitCode() << errorMsg;
        return false;
    }

    return true;
}

void ShredHelper::updateProgress(const QString &output)
{
    if (output.isEmpty())
        return;

    QString logOutput = output.trimmed();
    //std::cout << "Raw shred output: " << logOutput.toStdString() << std::endl;

    int deletedCount = output.count("已删除");
    if (deletedCount > 0) {
        m_processedFiles += deletedCount;
    }

    // 修改正则表达式以更准确地匹配 shred 输出格式
    QRegExp rx("shred:\\s*([^：]+)：第(\\d+)\\s*次，共(\\d+)\\s*次\\s*\\(random\\)\\.\\.\\.((?:[\\.\\d]+[GM]iB/[\\.\\d]+[GM]iB\\s*)?(\\d+)?%)?");

    if (rx.indexIn(output) == -1) {
        //std::cout << "Failed to match progress pattern" << std::endl;
        return;
    }

    // 从正则表达式捕获组中获取信息
    QString filePath = rx.cap(1).trimmed();   // 添加 trimmed() 去除可能的空白字符
    int currentPass = rx.cap(2).toInt();
    int totalPasses = rx.cap(3).toInt();

    // 获取文件名
    QFileInfo fileInfo(filePath);
    QString currentFile = fileInfo.fileName();

    fmDebug() << "Extracted file path: " << filePath;
    //std::cout << "Extracted file name: " << currentFile.toStdString() << std::endl;

    int totalProgress = 0;

    // 如果总文件数为1，说明是单文件模式
    if (m_totalFiles == 1) {
        // 获取进度百分比
        int currentPassProgress = rx.cap(5).isEmpty() ? 0 : rx.cap(5).toInt();

        if (!rx.cap(4).isEmpty()) {
            // 大文件格式，使用详细进度
            // 计算单个文件的总体进度
            double fileProgress = ((currentPass - 1) * 100.0 / totalPasses)   // 已完成次数的进度
                    + (currentPassProgress / 100.0) * (100.0 / totalPasses);   // 当前次数的进度
            totalProgress = static_cast<int>(fileProgress);
        } else {
            // 小文件格式，只按照当前pass计算
            totalProgress = ((currentPass - 1) * 100) / totalPasses;
        }

        //        std::cout << "Single file progress details:"
        //                  << "\n    - Current file: " << currentFile.toStdString()
        //                  << "\n    - Current pass: " << currentPass
        //                  << "\n    - Total passes: " << totalPasses;

        if (!rx.cap(4).isEmpty()) {
            std::cout << "\n    - Size info: " << rx.cap(4).toStdString();
        }

        fmDebug() << "\n    - Total progress: " << totalProgress << "%";
    } else {
        totalProgress = (m_processedFiles * 100) / m_totalFiles;

        fmDebug() << "Multi-file progress details:"
                  << "\n    - Current file: " << currentFile
                  << "\n    - Processed files: " << m_processedFiles
                  << "\n    - Total files: " << m_totalFiles
                  << "\n    - Total progress: " << totalProgress << "%";
    }

    totalProgress = std::min(totalProgress, 99);
    m_progressDialog->updateProgressValue(totalProgress, currentFile);
}

void ShredHelper::updateVaultMenuConfig()
{
    const QString kVaultDConfigName = "org.deepin.dde.file-manager.vault";

    // 获取现有的菜单配置
    const QVariant vRe = DConfigManager::instance()->value(kVaultDConfigName, "normalMenuActions");
    QStringList currentConfig = vRe.toStringList();
    fmDebug() << "Current vault menu config: " << currentConfig;

    if (!currentConfig.contains("shredfile")) {
        // 找到 "reverse-select" 的位置
        int reverseSelectIndex = currentConfig.indexOf("reverse-select");

        if (reverseSelectIndex != -1) {
            // 在 "reverse-select" 后面插入 "shredfile"
            currentConfig.insert(reverseSelectIndex + 1, "separator-line");
            currentConfig.insert(reverseSelectIndex + 2, "shredfile");
        } else {
            // 果找不到 "reverse-select"，就追加到末尾
            currentConfig.append("shredfile");
        }

        fmDebug() << "New config to be set: " << currentConfig;

        // 使用 DConfigManager 写入新配置
        DConfigManager::instance()->setValue(kVaultDConfigName, "normalMenuActions", currentConfig);
    }
}

bool ShredHelper::isPipe(const QString &path) const
{
    struct stat sb;
    if (stat(path.toLocal8Bit().constData(), &sb) == -1) {
        return false;
    }
    return S_ISFIFO(sb.st_mode);
}

void ShredHelper::debug(const std::string &message)
{
    fmDebug() << QString::fromStdString(message);
}

}
// namespace dfm_extenison_shred
