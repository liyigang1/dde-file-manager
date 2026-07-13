// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef BASEITEMDELEGATE_P_H
#define BASEITEMDELEGATE_P_H

#include "dfmplugin_workspace_global.h"

#include <dfm-base/utils/elidetextlayout.h>
#include <dfm-base/dfm_global_defines.h>

#include <QModelIndex>
#include <QStyleOptionViewItem>
#include <QSize>
#include <QtGlobal>
#include <QMutex>
#include <QTextOption>
#include <QStringList>
#include <QColor>
#include <QCache>
#include <QUrl>
#include <QPixmap>
#include <QIcon>

#include <memory>

QT_BEGIN_NAMESPACE
class QLineEdit;
QT_END_NAMESPACE

namespace dfmplugin_workspace {

class AbstractItemPaintProxy;
class FileViewHelper;
class BaseItemDelegate;
class BaseItemDelegatePrivate
{
public:
    explicit BaseItemDelegatePrivate(BaseItemDelegate *qq);
    virtual ~BaseItemDelegatePrivate();

    void init();

    void setupElideLayout(dfmbase::ElideTextLayout *layout,
                          const QString &text,
                          QTextOption::WrapMode wrapMode,
                          int lineHeight,
                          int alignment,
                          QPainter *painter,
                          bool highlightEnabled,
                          const QStringList &keywords,
                          const QColor &highlightColor) const;

    int textLineHeight { -1 };
    QSize itemSizeHint;
    mutable QModelIndex editingIndex;
    mutable QLineEdit *editor = nullptr;
    mutable quint64 editingSessionId { 0 }; // Unique ID for each editing session, used to handle abnormal cases like view destruction

    AbstractItemPaintProxy *paintProxy { nullptr };
    QWidget *commitDataCurentWidget { nullptr };
    int currentHeightLevel { 1 };

    // reusable ElideTextLayout for paint optimization, avoids repeated new/delete per paint cycle
    mutable std::unique_ptr<dfmbase::ElideTextLayout> reusableElideLayout { nullptr };

    void clearIconEmblemsCache();

    void removeIconEmblemsCache(const QList<QUrl> &urls);

    const QPixmap *getIconEmblemsCache(const QUrl &url) const;

    void cacheIconEmblems(const QUrl &url, QPixmap *pixmap) const;

    QPixmap *createCachedPixmap(const QRectF &iconRect, qreal dpr,
                                qreal padW, qreal padH) const;

    static constexpr qreal kEmblemPaddingRatio = 6.0;
    static constexpr int kMaxIconEmblemsCacheSize = 300;

    mutable QCache<QUrl, QPixmap> iconEmblemsCache { kMaxIconEmblemsCacheSize };

    BaseItemDelegate *q_ptr;
    Q_DECLARE_PUBLIC(BaseItemDelegate)
};

}

#endif   // BASEITEMDELEGATE_P_H
