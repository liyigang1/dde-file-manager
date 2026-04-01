// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "filelistdialog.h"

#include <dfm-io/dfile.h>

#include <QFileInfo>

#include <DListWidget>

using namespace dfm_extenison_shred;
DWIDGET_USE_NAMESPACE

FileListDialog::FileListDialog(const QStringList &paths, QWidget *parent)
    : DDialog(parent), filePaths(paths)
{
    initUi();
    initConnect();
}

void FileListDialog::initUi()
{
    setIcon(QIcon::fromTheme("dialog-warning"));

    int count { filePaths.size() };
    QString title = tr("Are you sure to shred these %1 items?").arg(count);
    QString message = tr("The file will be completely deleted and cannot be recovered.");
    setTitle(title);
    setMessage(message);

    QListWidget *fileList = new QListWidget(this);
    fileList->setStyleSheet("QListWidget { \
        background-color: white; \
        border-radius: 8px; \
        padding: 5px; \
    }");

    for (int i = 0; i < filePaths.size(); ++i) {
        QIcon icon;
        QFileInfo info(filePaths[i]);
        // Using the `exists` function of `QFileInfo` to
        // check for invalid link files is incorrect
        if (!dfmio::DFile(filePaths[i]).exists()) {
            continue;
        }

        if (info.isDir()) {
            icon = QIcon::fromTheme("folder");
        } else {
            icon = QIcon::fromTheme("application-json");
        }

        QListWidgetItem *item = new QListWidgetItem(icon, info.fileName());
        item->setSizeHint(QSize(item->sizeHint().width(), 35));  // 设置item高度
        fileList->addItem(item);
    }
    addContent(fileList);

    addButton(tr("Cancel"));
    addButton(tr("Shred"), false, ButtonWarning);
}

void FileListDialog::initConnect()
{
    connect(this, &FileListDialog::buttonClicked, this, &FileListDialog::handleBtnClicked);
}

void FileListDialog::handleBtnClicked(int index, const QString &text)
{
    Q_UNUSED(text);
    if (index == 0) {
        emit rejected();
    } else {
        emit accepted();
    }
}
