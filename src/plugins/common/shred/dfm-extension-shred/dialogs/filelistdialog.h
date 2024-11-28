// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FILELISTDIALOG_H
#define FILELISTDIALOG_H

#include <DDialog>

namespace dfm_extenison_shred {
class FileListDialog : public DTK_WIDGET_NAMESPACE::DDialog
{
    Q_OBJECT

public:
    explicit FileListDialog(const QStringList &paths, QWidget *parent = Q_NULLPTR);

public Q_SLOTS:
    void handleBtnClicked(int index, const QString &text);

Q_SIGNALS:
    void sigShredFile();

private:
    void initUi();
    void initConnect();

    QStringList filePaths;

};
}

#endif
