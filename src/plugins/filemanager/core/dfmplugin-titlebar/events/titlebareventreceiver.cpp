// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "titlebareventreceiver.h"
#include "views/titlebarwidget.h"
#include "views/navwidget.h"
#include "utils/crumbmanager.h"
#include "utils/crumbinterface.h"
#include "utils/titlebarhelper.h"
#include "utils/optionbuttonmanager.h"

#include <dfm-framework/event/eventhelper.h>

using namespace dfmplugin_titlebar;
TitleBarEventReceiver *TitleBarEventReceiver::instance()
{
    // 静态变量再堆上分配，最后析构不会有顺序问题
    static TitleBarEventReceiver *receiver = new TitleBarEventReceiver;
    return receiver;
}

void TitleBarEventReceiver::handleTabAdded(quint64 windowId)
{
    TitleBarWidget *w = TitleBarHelper::findTileBarByWindowId(windowId);
    if (!w)
        return;
    w->navWidget()->addHistroyStack();
}

void TitleBarEventReceiver::handleTabChanged(quint64 windowId, int index)
{
    TitleBarWidget *w = TitleBarHelper::findTileBarByWindowId(windowId);
    if (!w)
        return;
    w->navWidget()->switchHistoryStack(index);
}

void TitleBarEventReceiver::handleTabMoved(quint64 windowId, int from, int to)
{
    TitleBarWidget *w = TitleBarHelper::findTileBarByWindowId(windowId);
    if (!w)
        return;
    w->navWidget()->moveNavStacks(from, to);
}

void TitleBarEventReceiver::handleTabRemovd(quint64 windowId, int index)
{
    TitleBarWidget *w = TitleBarHelper::findTileBarByWindowId(windowId);
    if (!w)
        return;
    w->navWidget()->removeNavStackAt(index);
}

bool TitleBarEventReceiver::handleCustomRegister(const QString &scheme, const QVariantMap &properties)
{
    Q_ASSERT(!scheme.isEmpty());
    if (CrumbManager::instance()->isRegisted(scheme)) {
        fmWarning() << "Crumb sechme " << scheme << "has been resigtered!";
        return false;
    }

    bool keepAddressBar { properties.value(CustomKey::kKeepAddressBar).toBool() };
    bool hideListViewBtn { properties.value(CustomKey::kHideListViewBtn).toBool() };
    bool hideIconViewBtn { properties.value(CustomKey::kHideIconViewBtn).toBool() };
    bool hideTreeViewBtn { properties.value(CustomKey::kHideTreeViewBtn).toBool() };
    bool hideDetailSpaceBtn { properties.value(CustomKey::kHideDetailSpaceBtn).toBool() };
    ViewModeUrlCallback modelViewUrlCallback = DPF_NAMESPACE::paramGenerator<ViewModeUrlCallback>(properties.value(CustomKey::kViewModeUrlCallback));

    int state { OptionButtonManager::kDoNotHide };
    if (hideListViewBtn)
        state |= OptionButtonManager::kHideListViewBtn;
    if (hideIconViewBtn)
        state |= OptionButtonManager::kHideIconViewBtn;
    if (hideTreeViewBtn)
        state |= OptionButtonManager::kHideTreeViewBtn;
    if (hideDetailSpaceBtn)
        state |= OptionButtonManager::kHideDetailSpaceBtn;
    if (state != OptionButtonManager::kDoNotHide)
        OptionButtonManager::instance()->setOptBtnVisibleState(scheme, static_cast<OptionButtonManager::OptBtnVisibleState>(state));

    CrumbManager::instance()->registerCrumbCreator(scheme, [=]() {
        CrumbInterface *interface = new CrumbInterface();
        interface->setSupportedScheme(scheme);
        interface->setKeepAddressBar(keepAddressBar);
        return interface;
    });

    if (modelViewUrlCallback)
        TitleBarHelper::registerViewModelUrlCallback(scheme, modelViewUrlCallback);

    return true;
}

void TitleBarEventReceiver::handleStartSpinner(quint64 windowId)
{
    TitleBarWidget *w = TitleBarHelper::findTileBarByWindowId(windowId);
    if (!w)
        return;
    w->startSpinner();
}

void TitleBarEventReceiver::handleStopSpinner(quint64 windowId)
{
    TitleBarWidget *w = TitleBarHelper::findTileBarByWindowId(windowId);
    if (!w)
        return;
    w->stopSpinner();
}

void TitleBarEventReceiver::handleShowFilterButton(quint64 windowId, bool visible)
{
    TitleBarWidget *w = TitleBarHelper::findTileBarByWindowId(windowId);
    if (!w)
        return;
    w->showSearchFilterButton(visible);
}

void TitleBarEventReceiver::handleViewModeChanged(quint64 windowId, int mode)
{
    TitleBarWidget *w = TitleBarHelper::findTileBarByWindowId(windowId);
    if (!w)
        return;

    w->setViewModeState(mode);
}

void TitleBarEventReceiver::handleSetNewWindowAndTabEnable(bool enable)
{
    TitleBarHelper::newWindowAndTabEnabled = enable;
}

void TitleBarEventReceiver::handleWindowForward(quint64 windowId)
{
    TitleBarWidget *w = TitleBarHelper::findTileBarByWindowId(windowId);
    if (!w)
        return;
    w->navWidget()->forward();
}

void TitleBarEventReceiver::handleWindowBackward(quint64 windowId)
{
    TitleBarWidget *w = TitleBarHelper::findTileBarByWindowId(windowId);
    if (!w)
        return;
    w->navWidget()->back();
}

void TitleBarEventReceiver::handleRemoveHistory(quint64 windowId, const QUrl &url)
{
    TitleBarWidget *w = TitleBarHelper::findTileBarByWindowId(windowId);
    if (!w)
        return;
    w->navWidget()->removeUrlFromHistoryStack(url);
}

TitleBarEventReceiver::TitleBarEventReceiver(QObject *parent)
    : QObject(parent)
{
}
