// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "abstractitempaintproxy.h"
#include "fileview.h"
#include "models/fileviewmodel.h"

#include <dfm-base/interfaces/fileinfo.h>
#include <dfm-base/utils/iconutils.h>

using namespace dfmplugin_workspace;
DFMBASE_USE_NAMESPACE
DFMGLOBAL_USE_NAMESPACE

AbstractItemPaintProxy::AbstractItemPaintProxy(QObject *parent)
    : QObject(parent)
{
}

void AbstractItemPaintProxy::drawIcon(QPainter *painter, QRectF *rect, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    Q_UNUSED(painter)
    Q_UNUSED(rect)
    Q_UNUSED(option)
    Q_UNUSED(index)
}

void AbstractItemPaintProxy::drawBackground(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    Q_UNUSED(painter)
    Q_UNUSED(option)
    Q_UNUSED(index)
}

void AbstractItemPaintProxy::drawText(QPainter *painter, QRectF *rect, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    Q_UNUSED(painter)
    Q_UNUSED(rect)
    Q_UNUSED(option)
    Q_UNUSED(index)
}

QRectF AbstractItemPaintProxy::rectByType(RectOfItemType type, const QModelIndex &index)
{
    Q_UNUSED(type)
    Q_UNUSED(index)

    return QRectF();
}

QList<QRect> AbstractItemPaintProxy::allPaintRect(const QStyleOptionViewItem &option, const QModelIndex &index)
{
    Q_UNUSED(option)
    Q_UNUSED(index)

    return QList<QRect>();
}

int AbstractItemPaintProxy::iconRectIndex()
{
    return 0;
}

void AbstractItemPaintProxy::setStyleProxy(QStyle *style)
{
    this->style = style;
}

bool AbstractItemPaintProxy::isThumnailIconIndex(const QModelIndex &index) const
{
    if (!index.isValid())
        return false;

    auto parent = dynamic_cast<FileView *>(this->parent());
    if (!parent || !parent->model())
        return false;

    return IconPainterUtils::isThumbnailIcon(parent->model()->fileInfo(index));
}
