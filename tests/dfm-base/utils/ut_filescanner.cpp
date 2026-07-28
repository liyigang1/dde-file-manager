// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <dfm-base/utils/filescanner.h>
#include <dfm-base/utils/fileutils.h>

#include <gtest/gtest.h>

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>

#include <sys/stat.h>
#include <unistd.h>

DFMBASE_USE_NAMESPACE

namespace {

FileScanner::ScanOptions fileOperationOptions()
{
    return FileScanner::ScanOption::IncludeSource
            | FileScanner::ScanOption::CollectFiles;
}

int indexOfUrl(const QList<QUrl> &urls, const QUrl &url)
{
    for (int i = 0; i < urls.count(); ++i) {
        if (urls.at(i) == url)
            return i;
    }

    return -1;
}

}   // namespace

class UT_FileScanner : public testing::Test
{
};

TEST_F(UT_FileScanner, collectFilesIncludesSourceAndKeepsParentBeforeChild)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());

    QDir root(tmp.path());
    ASSERT_TRUE(root.mkpath("parent/child"));

    QFile file(root.filePath("parent/child/data.txt"));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(file.write("abc"), qint64(3));
    file.close();

    const auto result = FileScanner::scanSync({ QUrl::fromLocalFile(root.filePath("parent")) }, fileOperationOptions());

    const QUrl parentUrl = QUrl::fromLocalFile(root.filePath("parent"));
    const QUrl childDirUrl = QUrl::fromLocalFile(root.filePath("parent/child"));
    const QUrl childFileUrl = QUrl::fromLocalFile(root.filePath("parent/child/data.txt"));

    EXPECT_TRUE(result.allFiles.contains(parentUrl));
    EXPECT_TRUE(result.allFiles.contains(childDirUrl));
    EXPECT_TRUE(result.allFiles.contains(childFileUrl));
    EXPECT_LT(indexOfUrl(result.allFiles, parentUrl), indexOfUrl(result.allFiles, childDirUrl));
    EXPECT_LT(indexOfUrl(result.allFiles, childDirUrl), indexOfUrl(result.allFiles, childFileUrl));
    EXPECT_EQ(result.fileCount, 1);
    EXPECT_EQ(result.directoryCount, 2);
}

TEST_F(UT_FileScanner, progressSizeUsesMemoryPageForDirectoriesAndEmptyFiles)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());

    QDir root(tmp.path());
    ASSERT_TRUE(root.mkpath("dir"));

    QFile empty(root.filePath("dir/empty.txt"));
    ASSERT_TRUE(empty.open(QIODevice::WriteOnly));
    empty.close();

    const auto result = FileScanner::scanSync({ QUrl::fromLocalFile(tmp.path()) }, fileOperationOptions());

    EXPECT_EQ(result.fileCount, 1);
    EXPECT_EQ(result.directoryCount, 2);
    EXPECT_GE(result.progressSize, FileUtils::getMemoryPageSize() * 3);
    EXPECT_GE(result.totalSize, qint64(0));
}

TEST_F(UT_FileScanner, noFollowSymlinkDoesNotDescendIntoLinkedDirectory)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());

    QDir root(tmp.path());
    ASSERT_TRUE(root.mkpath("target"));

    QFile child(root.filePath("target/child.txt"));
    ASSERT_TRUE(child.open(QIODevice::WriteOnly));
    ASSERT_EQ(child.write("child"), qint64(5));
    child.close();

    const QString linkPath = root.filePath("link-to-target");
    ASSERT_EQ(::symlink(root.filePath("target").toUtf8().constData(), linkPath.toUtf8().constData()), 0);

    const auto result = FileScanner::scanSync({ QUrl::fromLocalFile(linkPath) }, fileOperationOptions());

    EXPECT_TRUE(result.allFiles.contains(QUrl::fromLocalFile(linkPath)));
    EXPECT_FALSE(result.allFiles.contains(QUrl::fromLocalFile(root.filePath("target/child.txt"))));
    EXPECT_EQ(result.fileCount, 0);
    EXPECT_EQ(result.directoryCount, 1);
    EXPECT_EQ(result.totalSize, qint64(0));
}

TEST_F(UT_FileScanner, fifoIsCountedAndCollectedButNotSized)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());

    QDir root(tmp.path());
    const QString fifoPath = root.filePath("pipe");
    ASSERT_EQ(::mkfifo(fifoPath.toUtf8().constData(), 0600), 0);

    const auto result = FileScanner::scanSync({ QUrl::fromLocalFile(tmp.path()) }, fileOperationOptions());

    EXPECT_TRUE(result.allFiles.contains(QUrl::fromLocalFile(fifoPath)));
    EXPECT_EQ(result.fileCount, 1);
    EXPECT_EQ(result.directoryCount, 1);
    EXPECT_EQ(result.totalSize, qint64(0));
}

class UT_FileScannerExcludePaths : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(tempDir.isValid());

        QDir root(tempDir.path());
        ASSERT_TRUE(root.mkpath("subdir1"));
        ASSERT_TRUE(root.mkpath("subdir2"));

        subdir1 = root.filePath("subdir1");
        subdir2 = root.filePath("subdir2");
        ASSERT_TRUE(writeFile(root.filePath("subdir1/file_a.txt")));
        ASSERT_TRUE(writeFile(root.filePath("subdir2/file_b.txt")));
        ASSERT_TRUE(writeFile(root.filePath("file_c.txt")));
    }

    static bool writeFile(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return false;
        return file.write("data") == 4;
    }

    bool scan(const QStringList &excludePaths,
              FileScanner::ScanResult *result,
              FileScanner::ScanOptions options = FileScanner::ScanOption::NoOption)
    {
        FileScanner scanner;
        scanner.setOptions(options);
        scanner.setExcludePaths(excludePaths);

        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

        bool completed = false;
        QObject::connect(&scanner, &FileScanner::finished, &loop,
                         [&](const FileScanner::ScanResult &scanResult) {
                             *result = scanResult;
                             completed = true;
                             loop.quit();
                         });

        scanner.start({ QUrl::fromLocalFile(tempDir.path()) });
        timeout.start(5000);
        if (!completed)
            loop.exec();
        return completed;
    }

    QTemporaryDir tempDir;
    QString subdir1;
    QString subdir2;
};

TEST_F(UT_FileScannerExcludePaths, excludesSingleDirectory)
{
    FileScanner::ScanResult result;
    ASSERT_TRUE(scan({ subdir1 }, &result));

    EXPECT_EQ(result.fileCount, 2);
    EXPECT_EQ(result.directoryCount, 1);
    EXPECT_EQ(result.totalSize, 8);
}

TEST_F(UT_FileScannerExcludePaths, excludesMultipleDirectories)
{
    FileScanner::ScanResult result;
    ASSERT_TRUE(scan({ subdir1, subdir2 }, &result));

    EXPECT_EQ(result.fileCount, 1);
    EXPECT_EQ(result.directoryCount, 0);
    EXPECT_EQ(result.totalSize, 4);
}

TEST_F(UT_FileScannerExcludePaths, normalizesTrailingSlashInCountOnlyScan)
{
    FileScanner::ScanResult result;
    ASSERT_TRUE(scan({ subdir1 + "/" }, &result, FileScanner::ScanOption::CountOnly));

    EXPECT_EQ(result.fileCount, 2);
    EXPECT_EQ(result.directoryCount, 1);
    EXPECT_EQ(result.totalSize, 0);
}

TEST_F(UT_FileScannerExcludePaths, excludesSourceDirectory)
{
    FileScanner::ScanResult result;
    ASSERT_TRUE(scan({ tempDir.path() + "/" }, &result));

    EXPECT_EQ(result.fileCount, 0);
    EXPECT_EQ(result.directoryCount, 0);
    EXPECT_EQ(result.totalSize, 0);
}

TEST_F(UT_FileScannerExcludePaths, storesConfiguredPaths)
{
    FileScanner scanner;
    const QStringList paths { "/tmp/foo", "/var/bar" };

    scanner.setExcludePaths(paths);
    EXPECT_EQ(scanner.excludePaths(), paths);

    scanner.setExcludePaths({});
    EXPECT_TRUE(scanner.excludePaths().isEmpty());
}
