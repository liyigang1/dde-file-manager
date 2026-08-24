// SPDX-FileCopyrightText: 2021 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "baseitemdelegate_p.h"
#include "views/baseitemdelegate.h"
#include "views/fileview.h"
#include "utils/fileviewhelper.h"

#include <QPainter>
#include <QAbstractItemView>
#include <QTextOption>

using namespace dfmplugin_workspace;
using namespace dfmbase;

BaseItemDelegatePrivate::BaseItemDelegatePrivate(BaseItemDelegate *qq)
    : q_ptr(qq)
{
    reusableElideLayout.reset(new ElideTextLayout);
}

BaseItemDelegatePrivate::~BaseItemDelegatePrivate()
{
}

void BaseItemDelegatePrivate::setupElideLayout(dfmbase::ElideTextLayout *layout,
                                                const QString &text,
                                                QTextOption::WrapMode wrapMode,
                                                int lineHeight,
                                                int alignment,
                                                QPainter *painter,
                                                bool highlightEnabled,
                                                const QStringList &keywords,
                                                const QColor &highlightColor) const
{
    layout->setText(text);
    layout->setAttribute(ElideTextLayout::kWrapMode, wrapMode);
    layout->setAttribute(ElideTextLayout::kLineHeight, lineHeight);
    layout->setAttribute(ElideTextLayout::kAlignment, alignment);
    layout->setAttribute(ElideTextLayout::kFont, painter->font());
    layout->setAttribute(ElideTextLayout::kTextDirection, painter->layoutDirection());
    layout->setHighlightEnabled(highlightEnabled);
    layout->setHighlightKeywords(keywords);
    layout->setHighlightColor(highlightColor);
    layout->setAttribute(ElideTextLayout::kBackgroundRadius, 0);
}

void BaseItemDelegatePrivate::init()
{
    Q_Q(BaseItemDelegate);

    q->connect(q, &BaseItemDelegate::commitData, q->parent(), &FileViewHelper::handleCommitData);
    q->connect(q->parent()->parent(), &QAbstractItemView::iconSizeChanged, q, &BaseItemDelegate::updateItemSizeHint);
}
