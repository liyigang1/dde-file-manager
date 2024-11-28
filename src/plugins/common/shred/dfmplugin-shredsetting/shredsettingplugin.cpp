// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "shredsettingplugin.h"
#include "utils/shredutils.h"

#include <dfm-base/settingdialog/settingjsongenerator.h>
#include <dfm-base/settingdialog/customsettingitemregister.h>

#define SHRED_SETTING_GROUP "10_advance.04_shred"
inline constexpr char kShredSettingGroup[] { SHRED_SETTING_GROUP };
inline constexpr char kShredSettingShred[] { "00_file_shred" };

using namespace dfmbase;
using namespace dfmplugin_shred;

void ShredPlugin::initialize()
{
}

bool ShredPlugin::start()
{
    ShredUtils::initDconfig();
    addShredSettingItem();
    return true;
}

void ShredPlugin::addShredSettingItem()
{
    SettingJsonGenerator::instance()->addGroup(kShredSettingGroup, tr("File shred"));

    CustomSettingItemRegister::instance()->registCustomSettingItemType("switchbutton", ShredUtils::createSettingButton);

    QVariantMap config {
        { "key", kShredSettingShred },
        { "text", tr("File shredding function") },
        { "type", "switchbutton" },
        { "default", false },
        { "message", tr("After opening, you can use \"File Smash\" in the right-click menu to completely delete files") },
    };

    QString key = QString("%1.%2").arg(kShredSettingGroup, kShredSettingShred);
    SettingJsonGenerator::instance()->addConfig(key, config);
}
