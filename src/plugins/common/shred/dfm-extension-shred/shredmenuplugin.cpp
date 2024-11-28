// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "shredmenuplugin.h"
#include "utils/shredhelper.h"

#include <sys/types.h>
#include <sys/xattr.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cassert>
#include <iostream>

#include <dfm-extension/menu/dfmextmenu.h>
#include <dfm-extension/menu/dfmextmenuproxy.h>
#include <dfm-extension/menu/dfmextaction.h>

namespace dfm_extenison_shred {
USING_DFMEXT_NAMESPACE

ShredMenuPlugin::ShredMenuPlugin()
    : DFMEXT::DFMExtMenuPlugin()
{
    registerInitialize([this](DFMEXT::DFMExtMenuProxy *proxy) {
        initialize(proxy);
    });
    registerBuildNormalMenu([this](DFMExtMenu *main, const std::string &currentPath,
                                   const std::string &focusPath, const std::list<std::string> &pathList,
                                   bool onDesktop) {
        return buildNormalMenu(main, currentPath, focusPath, pathList, onDesktop);
    });
}

ShredMenuPlugin::~ShredMenuPlugin()
{
}

void ShredMenuPlugin::initialize(DFMExtMenuProxy *proxy)
{
    m_proxy = proxy;

    ShredHelper::initDconfig();
}

bool ShredMenuPlugin::buildNormalMenu(DFMExtMenu *main, const std::string &currentPath,
                                      const std::string &focusPath, const std::list<std::string> &pathList,
                                      bool onDesktop)
{
    // 首先检查功能是否启用
    if (!ShredHelper::isShredEnabled()) {
        ShredHelper::debug("Shred function is disabled");
        return true;
    }
    ShredHelper::updateVaultMenuConfig();

    // 检查 pathList 是否为空
    if (pathList.empty())
        return true;

    // 检查所有路径是否都在用户目录内部
    bool allPathsValid = true;
    for (const auto &path : pathList) {
        std::string cleanPath = removeScheme(path);
        if (!ShredHelper::isValidPath(cleanPath)) {
            allPathsValid = false;
            break;
        }
    }

    if (!allPathsValid) {
        return true;
    }

    // 1. 创建粉碎文件菜单项
    auto rootAction { m_proxy->createAction() };
    ShredHelper helper;
    rootAction->setText(helper.actionName().toStdString());
    rootAction->setProperty("actionID", "shredfile");
    rootAction->registerTriggered([pathList](DFMExtAction *, bool) {
        ShredHelper helper;
        std::vector<std::string> validPaths;

        // 收集所有有效路径
        for (const auto &path : pathList) {
            std::string cleanPath = removeScheme(path);
            if (helper.isValidPath(cleanPath)) {
                validPaths.push_back(cleanPath);
            }
        }

        // 批量处理所有文件
        if (!validPaths.empty()) {
            helper.shredfile(validPaths);
        }
    });

    // 2. 创建分隔符
    auto separator = m_proxy->createAction();
    separator->setSeparator(true);

    auto separator2 = m_proxy->createAction();
    separator2->setSeparator(true);

    // 3. 插入菜单项
    DFMExtAction *beforeAction = nullptr;
    auto actions = main->actions();

    // 使用 property 方式查找 send-to action
    for (const auto &action : actions) {
        auto res = action->property("actionID") == "send-to";
        if (res) {
            beforeAction = action;
            break;
        }
    }

    if (beforeAction) {
        main->insertAction(beforeAction, separator);
        main->insertAction(separator, rootAction);
        main->insertAction(rootAction, separator2);
    } else
        main->addAction(rootAction);

    return true;
}

// 在 V5 版本的文管中，menu 接收到的是文件 url 的字符串，而 V6 则直接是文件路径
std::string ShredMenuPlugin::removeScheme(const std::string &url)
{
    std::string result = url;

    // 查找第一个冒号后的斜杠位置
    size_t startPos = result.find("://");
    if (startPos != std::string::npos) {
        startPos = result.find('/', startPos + 3);   // 跳过冒号和两个斜杠
        if (startPos != std::string::npos) {
            result = result.substr(startPos);
        }
    }

    return result;
}

}   // namespace dfm_extenison_shred
