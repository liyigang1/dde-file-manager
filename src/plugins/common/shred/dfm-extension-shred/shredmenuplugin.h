// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SHREDMENUPLUGIN_H
#define SHREDMENUPLUGIN_H

#include <dfm-extension/menu/dfmextmenuplugin.h>

namespace dfm_extenison_shred {

class ShredMenuPlugin : public DFMEXT::DFMExtMenuPlugin
{
public:
    ShredMenuPlugin();
    ~ShredMenuPlugin();

    void initialize(DFMEXT::DFMExtMenuProxy *proxy) DFM_FAKE_OVERRIDE;
    bool buildNormalMenu(DFMEXT::DFMExtMenu *main,
                         const std::string &currentPath,
                         const std::string &focusPath,
                         const std::list<std::string> &pathList,
                         bool onDesktop) DFM_FAKE_OVERRIDE;

private:
    static std::string removeScheme(const std::string &url);

private:
    DFMEXT::DFMExtMenuProxy *m_proxy { nullptr };
};

}   // namespace dfm_extenison_shred
#endif   // SHREDMENUPLUGIN_H

