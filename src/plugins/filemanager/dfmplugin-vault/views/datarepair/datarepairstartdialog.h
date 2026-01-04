// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DATAREPAIRSTARTDIALOG_H
#define DATAREPAIRSTARTDIALOG_H

#include <DDialog>

namespace dfmplugin_vault {

class DataRepairStartDialog : public DTK_WIDGET_NAMESPACE::DDialog
{
    Q_OBJECT

public:
    explicit DataRepairStartDialog(const QString &baseDir,
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
