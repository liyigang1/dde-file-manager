// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DATAREPAIRLOCKFAILEDDIALOG_H
#define DATAREPAIRLOCKFAILEDDIALOG_H

#include <DDialog>

namespace dfmplugin_vault {

class DataRepairLockFailedDialog : public DTK_WIDGET_NAMESPACE::DDialog
{
    Q_OBJECT

public:
    explicit DataRepairLockFailedDialog(const QString &baseDir,
                                        QWidget *parent = Q_NULLPTR);

public Q_SLOTS:
    void onButtonClicked(int index, const QString &text);

private:
    void initUI();
    void initConnect();

    QString m_baseDirPath;
};

}

#endif
