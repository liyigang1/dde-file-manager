// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FILEMONITORPLUGIN_H
#define FILEMONITORPLUGIN_H

#include "ddplugin_filemonitor_global.h"

#include <dfm-framework/dpf.h>

DDP_FILEMONITOR_BEGIN_NAMESPACE

class BackgroundPlugin : public dpf::Plugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.deepin.plugin.desktop" FILE "filemonitor.json")

public:
    virtual void initialize() override;
    virtual bool start() override;
    virtual void stop() override;
private:
    QThread *workThread { nullptr };
};

DDP_FILEMONITOR_END_NAMESPACE

#endif   // FILEMONITORPLUGIN_H
