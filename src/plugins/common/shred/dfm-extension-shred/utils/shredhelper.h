// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SHREDHELPER_H
#define SHREDHELPER_H

#include <QObject>
#include <QString>
#include <vector>
#include <string>

namespace dfm_extenison_shred {
class ProgressDialog;
class ShredHelper : public QObject
{
    Q_OBJECT

public:
    explicit ShredHelper(QObject *parent = nullptr);
    ~ShredHelper();

    QString actionName();

    static bool isShredEnabled();
    static void initDconfig();
    static bool isValidPath(const std::string &path);

    void shredfile(const std::vector<std::string> &filePaths);
    void shredfile(const std::string &filePath);

    int countFilesInDirectory(const QString &dirPath);
    static void updateVaultMenuConfig();

    static void debug(const std::string &message);

private:
    void initProgressDialog();
    void processDirectory(const QString &dirPath);
    void updateProgress(const QString &output);
    bool executeShredCommandBatch(const QStringList &filePaths);

private:
    bool isPipe(const QString &path) const;

private:
    ProgressDialog *m_progressDialog { nullptr };
    int m_totalFiles;
    int m_processedFiles;
};

}   // namespace dfm_extenison_shred

#endif   // SHREDHELPER_H
