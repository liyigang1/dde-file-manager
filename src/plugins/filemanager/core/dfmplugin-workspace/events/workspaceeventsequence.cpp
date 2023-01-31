/*
 * Copyright (C) 2022 Uniontech Software Technology Co., Ltd.
 *
 * Author:     liuyangming<liuyangming@uniontech.com>
 *
 * Maintainer: zhengyouge<zhengyouge@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "workspaceeventsequence.h"

#include <dfm-framework/dpf.h>

#include <QPainter>
#include <QRectF>
#include <QAbstractItemView>
#include <QList>

Q_DECLARE_METATYPE(QRectF *)
Q_DECLARE_METATYPE(QPainter *)
Q_DECLARE_METATYPE(Qt::DropAction *)
Q_DECLARE_METATYPE(QList<QAbstractItemView::SelectionMode> *)
Q_DECLARE_METATYPE(dfmbase::ElideTextLayout *)

DPF_USE_NAMESPACE
DFMGLOBAL_USE_NAMESPACE
using namespace dfmplugin_workspace;

static constexpr char kCurrentEventSpace[] { DPF_MACRO_TO_STR(DPWORKSPACE_NAMESPACE) };

WorkspaceEventSequence *WorkspaceEventSequence::instance()
{
    static WorkspaceEventSequence ins;
    return &ins;
}

bool WorkspaceEventSequence::doPaintListItem(int role, const QUrl &url, QPainter *painter, QRectF *rect)
{
    return dpfHookSequence->run(kCurrentEventSpace, "hook_Delegate_PaintListItem", role, url, painter, rect);
}

bool WorkspaceEventSequence::doPaintIconItemText(const QUrl &url, const QRectF &rect, QPainter *painter, dfmbase::ElideTextLayout *layout)
{
    return dpfHookSequence->run(kCurrentEventSpace, "hook_Delegate_PaintIconItem", url, rect, painter, layout);
}

bool WorkspaceEventSequence::doCheckDragTarget(const QList<QUrl> &urls, const QUrl &urlTo, Qt::DropAction *action)
{
    return dpfHookSequence->run(kCurrentEventSpace, "hook_DragDrop_CheckDragDropAction", urls, urlTo, action);
}

bool WorkspaceEventSequence::doFetchSelectionModes(const QUrl &url, QList<QAbstractItemView::SelectionMode> *modes)
{
    return dpfHookSequence->run(kCurrentEventSpace, "hook_View_FetchSupportSelectionModes", url, modes);
}

bool WorkspaceEventSequence::doFetchCustomColumnRoles(const QUrl &rootUrl, QList<ItemRoles> *roleList)
{
    return dpfHookSequence->run(kCurrentEventSpace, "hook_Model_FetchCustomColumnRoles", rootUrl, roleList);
}

bool WorkspaceEventSequence::doFetchCustomRoleDiaplayName(const QUrl &rootUrl, const ItemRoles role, QString *displayName)
{
    return dpfHookSequence->run(kCurrentEventSpace, "hook_Model_FetchCustomRoleDisplayName", rootUrl, role, displayName);
}

bool WorkspaceEventSequence::doCheckTransparent(const QUrl &url, TransparentStatus *status)
{
    return dpfHookSequence->run(kCurrentEventSpace, "hook_Delegate_CheckTransparent", url, status);
}

WorkspaceEventSequence::WorkspaceEventSequence(QObject *parent)
    : QObject(parent)
{
}
