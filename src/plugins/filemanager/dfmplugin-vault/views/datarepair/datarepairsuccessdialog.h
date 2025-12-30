// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DATAREPAIRSUCCESSDIALOG_H
#define DATAREPAIRSUCCESSDIALOG_H

#include <DDialog>

namespace dfmplugin_vault {

class DataRepairSuccessDialog : public DTK_WIDGET_NAMESPACE::DDialog
{
    Q_OBJECT

public:
    explicit DataRepairSuccessDialog(QWidget *parent =  Q_NULLPTR);

private:
    void initUI();
};

}

#endif
