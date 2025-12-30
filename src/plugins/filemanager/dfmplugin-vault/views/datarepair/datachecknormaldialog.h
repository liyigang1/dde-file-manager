// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DATACHECKNORMALDIALOG_H
#define DATACHECKNORMALDIALOG_H

#include "dfmplugin_vault_global.h"

#include <DDialog>

namespace dfmplugin_vault {

class DataCheckNormalDialog : public DTK_WIDGET_NAMESPACE::DDialog
{
    Q_OBJECT

public:
    explicit DataCheckNormalDialog(QWidget *parent = Q_NULLPTR);

private:
    void initUI();
};

}

#endif
