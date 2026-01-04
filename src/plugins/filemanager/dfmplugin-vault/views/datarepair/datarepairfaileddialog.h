// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef datarepairfaileddialog_h
#define datarepairfaileddialog_h

#include <DDialog>

namespace dfmplugin_vault {

class DataRepairFailedDialog : public DTK_WIDGET_NAMESPACE::DDialog
{
    Q_OBJECT

public:
    explicit DataRepairFailedDialog(QWidget *parent = Q_NULLPTR);

public Q_SLOTS:
    void onButtonClicked(int index, const QString &text);

private:
    void initUI();
    void initConnect();
};

}

#endif
