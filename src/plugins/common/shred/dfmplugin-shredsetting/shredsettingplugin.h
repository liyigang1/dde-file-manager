// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SHREDPLUGIN_H
#define SHREDPLUGIN_H

#include <dfm-framework/dpf.h>

namespace dfmplugin_shred {
class ShredPlugin : public dpf::Plugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.deepin.plugin.common" FILE "shredsettingplugin.json")

    // Plugin interface
public:
    virtual void initialize() override;
    virtual bool start() override;

private:
    void addShredSettingItem();
};

}   // namespace dfmplugin_shred

#endif   // SHREDPLUGIN_H
