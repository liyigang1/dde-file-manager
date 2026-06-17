// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "iconpainterutils.h"

#include <dfm-base/utils/iconutils.h>
#include <dfm-base/utils/iconcachemanager.h>

#include <QApplication>
#include <QPainterPath>

#include <cmath>

#define CEIL(x) (static_cast<int>(std::ceil(x)))

DFMBASE_USE_NAMESPACE

/*!
 * \brief visualAlignment 文本对齐辅助函数
 */
Qt::Alignment IconPainterUtils::visualAlignment(Qt::LayoutDirection direction, Qt::Alignment alignment)
{
    if (!(alignment & Qt::AlignHorizontal_Mask))
        alignment |= Qt::AlignLeft;
    if (!(alignment & Qt::AlignAbsolute) && (alignment & (Qt::AlignLeft | Qt::AlignRight))) {
        if (direction == Qt::RightToLeft)
            alignment ^= (Qt::AlignLeft | Qt::AlignRight);
        alignment |= Qt::AlignAbsolute;
    }
    return alignment;
}

/*!
 * \brief getIconPixmap 基于 iconName 获取 icon 的 pixmap（带 QPixmapCache 缓存）
 */
QPixmap IconPainterUtils::getIconPixmap(const QString &iconName, const QSize &size,
                                        qreal pixelRatio, QIcon::Mode mode, QIcon::State state)
{
    if (iconName.isEmpty() || size.width() <= 0 || size.height() <= 0)
        return QPixmap();

    QPixmap px = IconCacheManager::getPixmap(iconName, size, mode, state);
    px.setDevicePixelRatio(pixelRatio);
    return px;
}

/*!
 * \brief getIconPixmap 基于 QIcon 获取 icon 的 pixmap（缩略图/兼容路径）
 */
QPixmap IconPainterUtils::getIconPixmap(const QIcon &icon, const QSize &size,
                                        qreal pixelRatio, QIcon::Mode mode, QIcon::State state)
{
    // 不用绘制空白的图片，使用unknown
    if (icon.isNull())
        return getIconPixmap("unknown", size, pixelRatio, mode, state);

    // 确保当前参数参入获取图片大小大于0
    if (size.width() <= 0 || size.height() <= 0)
        return QPixmap();

    auto px = icon.pixmap(size, mode, state);
    if (px.isNull() || px.size().isEmpty())
        return QPixmap();

    px.setDevicePixelRatio(pixelRatio);

    return px;
}

/*!
 * \brief paintIcon 绘制指定区域内 icon 的 pixmap
 *
 * \return 成功绘制返回 painted rect；失败返回 std::nullopt
 */
std::optional<QRect> IconPainterUtils::paintIcon(QPainter *painter, const QIcon &icon, const PaintIconOpts &opts)
{
    // Copy of QStyle::alignedRect
    Qt::Alignment alignment { visualAlignment(painter->layoutDirection(), opts.alignment) };
    const qreal pixelRatio = painter->device()->devicePixelRatioF();

    // 主题图标：使用 iconName 走 QPixmapCache 缓存路径
    QPixmap px;
    if (!opts.isThumb && !opts.iconName.isEmpty()) {
        px = getIconPixmap(opts.iconName, opts.rect.size().toSize(), pixelRatio, opts.mode, opts.state);
    } else {
        px = getIconPixmap(icon, opts.rect.size().toSize(), pixelRatio, opts.mode, opts.state);
    }

    // 缩略图缩放到指定的size，绘制不出来就直接返回，绘制fileicon
    if (px.isNull() && opts.isThumb)
        return std::nullopt;

    qreal x = opts.rect.x();
    qreal y = opts.rect.y();
    qreal w = px.width() / px.devicePixelRatio();
    qreal h = px.height() / px.devicePixelRatio();

    if ((alignment & Qt::AlignVCenter) == Qt::AlignVCenter)
        y += (opts.rect.size().height() - h) / 2.0;
    else if ((alignment & Qt::AlignBottom) == Qt::AlignBottom)
        y += opts.rect.size().height() - h;
    if ((alignment & Qt::AlignRight) == Qt::AlignRight)
        x += opts.rect.size().width() - w;
    else if ((alignment & Qt::AlignHCenter) == Qt::AlignHCenter)
        x += (opts.rect.size().width() - w) / 2.0;

    // Task: 337513 — 缩略图模式下绘制带阴影背景
    if (opts.viewMode == ViewMode::kIconMode && opts.isThumb) {
        painter->save();
        painter->setRenderHints(painter->renderHints() | QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);

        auto iconStyle { IconUtils::getIconStyle(opts.rect.size().toSize().width()) };
        QRect backgroundRect { qRound(x), qRound(y), qRound(w), qRound(h) };
        QRect imageRect { backgroundRect };

        // 绘制带有阴影的背景
        auto stroke { iconStyle.stroke };
        backgroundRect.adjust(-stroke, -stroke, stroke, stroke);
        const auto &originPixmap { IconUtils::renderIconBackground(backgroundRect.size(), iconStyle) };
        const auto &shadowPixmap { IconUtils::addShadowToPixmap(originPixmap, iconStyle.shadowOffset, iconStyle.shadowRange, 0.2) };
        painter->drawPixmap(backgroundRect, shadowPixmap);
        imageRect.adjust(iconStyle.shadowRange, iconStyle.shadowRange, -iconStyle.shadowRange, -iconStyle.shadowRange);

        QPainterPath clipPath;
        auto radius { iconStyle.radius - iconStyle.stroke };
        clipPath.addRoundedRect(imageRect, radius, radius);
        painter->setClipPath(clipPath);
        painter->drawPixmap(imageRect, px);
        painter->restore();

        return backgroundRect;
    }

    painter->drawPixmap(qRound(x), qRound(y), px);
    // return rect before scale
    return QRect(qRound(x), qRound(y), CEIL(w), CEIL(h));
}

/*!
 * \brief isThumbnailIcon 判断文件是否已生成缩略图
 *
 * 排除 AppImage 文件（此类文件不应展示缩略图）。
 */
bool IconPainterUtils::isThumbnailIcon(const FileInfoPointer &info)
{
    if (!info)
        return false;

    if (info->nameOf(NameInfoType::kMimeTypeName) == Global::Mime::kTypeAppAppimage)
        return false;

    const auto &attribute { info->extendAttributes(ExtInfoType::kFileThumbnail) };
    if (attribute.isValid() && !attribute.value<QIcon>().isNull())
        return true;

    return false;
}
