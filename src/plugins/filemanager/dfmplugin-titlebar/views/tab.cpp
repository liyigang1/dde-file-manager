// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "tab.h"
#include "private/tab_p.h"

#include <dfm-base/interfaces/abstractbaseview.h>
#include <dfm-base/base/schemefactory.h>
#include <dfm-base/utils/fileutils.h>
#include <dfm-base/utils/systempathutil.h>
#include <dfm-framework/event/event.h>
#include <dfm-framework/dpf.h>

#include <DWidgetUtil>
#include <DPalette>
#include <DPaletteHelper>
#include <DGuiApplicationHelper>

#include <qdrawutil.h>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include <QDrag>
#include <QMimeData>
#include <QApplication>

DWIDGET_USE_NAMESPACE
DFMBASE_USE_NAMESPACE

DPTITLEBAR_USE_NAMESPACE

TabPrivate::TabPrivate()
{
}

Tab::Tab(QGraphicsObject *parent)
    : QGraphicsObject(parent),
      d(new TabPrivate())
{
    setAcceptHoverEvents(true);
    setFlags(ItemIsSelectable);
    setAcceptedMouseButtons(Qt::LeftButton);
}

QUrl Tab::getCurrentUrl() const
{
    return d->url;
}

void Tab::setCurrentUrl(const QUrl &url)
{
    d->url = url;

    QString fileName = getDisplayNameByUrl(url);
    d->tabAlias.clear();
    dpfHookSequence->run("dfmplugin_titlebar", "hook_Tab_SetTabName", url, &d->tabAlias);

    setTabText(fileName);
}

void Tab::setTabText(const QString &text)
{
    d->tabText = text;
    update();
}

void Tab::setTabAlias(const QString &alias)
{
    d->tabAlias = alias;
    update();
}

bool Tab::isChecked() const
{
    return d->checked;
}

void Tab::setChecked(const bool check)
{
    d->checked = check;
    update();
}

int Tab::width() const
{
    return d->width;
}

int Tab::height() const
{
    return d->height;
}

void Tab::setGeometry(const QRect &rect)
{
    prepareGeometryChange();
    setX(rect.x());
    setY(rect.y());
    d->width = rect.width();
    d->height = rect.height();
}

QRect Tab::geometry() const
{
    return QRect(static_cast<int>(x()), static_cast<int>(y()), d->width, d->height);
}

bool Tab::isDragging() const
{
    return d->isDragging;
}

void Tab::setCanDrag(bool canDrag)
{
    d->canDrag = canDrag;
}

void Tab::setHovered(bool hovered)
{
    d->hovered = hovered;
}

bool Tab::isDragOutSide()
{
    return d->dragOutSide;
}

bool Tab::borderLeft() const
{
    return d->borderLeft;
}

void Tab::setBorderLeft(const bool flag)
{
    d->borderLeft = flag;
}

QRectF Tab::boundingRect() const
{
    return QRectF(0, 0, d->width, d->height);
}

QPainterPath Tab::shape() const
{
    QPainterPath path;
    path.addRect(boundingRect());
    return path;
}

void Tab::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)

    if (d->dragOutSide)
        return;

    QPen pen = painter->pen();
    pen.setWidth(1);

    QFont font;
    int tabMargin = 10;
    int blueSquareWidth = isChecked() ? 6 : 0;
    int blueSquareMargin = isChecked() ? 4 : 0;
    int buttonSize = (d->hovered && d->showCloseButton) ? 16 : 0;
    int buttonMargin = (d->hovered && d->showCloseButton) ? 4 : 0;

    int textMargin = blueSquareWidth + blueSquareMargin;

    painter->setFont(font);
    QFontMetrics fm(font);
    QString tabName = d->tabAlias.isEmpty() ? d->tabText : d->tabAlias;

    // 1. 计算文本宽度时不考虑按钮空间，这样文本位置在hover和非hover时保持一致
    QString str = fm.elidedText(tabName, Qt::ElideRight, d->width - tabMargin - textMargin);

    // 2. 计算文本绘制位置，保持居中
    int textX = (d->width - fm.horizontalAdvance(str) - textMargin) / 2 + textMargin;

    // 3. 如果文本会与关闭按钮重叠，重新计算文本宽度
    if (d->hovered && d->showCloseButton) {
        int textEndX = textX + fm.horizontalAdvance(str);
        int buttonStartX = d->width - buttonSize - buttonMargin;
        if (textEndX > buttonStartX) {
            // 重新计算文本宽度，确保不重叠
            str = fm.elidedText(tabName, Qt::ElideRight, buttonStartX - textX);
        }
    }

    DPalette pal = DPaletteHelper::instance()->palette(widget);
    QColor color;

    // draw backgound
    if (d->hovered) {
        if (DGuiApplicationHelper::instance()->themeType() == DGuiApplicationHelper::LightType)
            color = QColor(0, 0, 0, 26);
        else
            color = QColor(255, 255, 255, 15);
        painter->fillRect(boundingRect(), color);

    } else {
        color = pal.color(QPalette::Active, QPalette::Base);
        painter->fillRect(boundingRect(), color);
    }

    // 绘制蓝色方块
    if (isChecked()) {
        painter->save();
        QColor blueColor = pal.color(QPalette::Active, QPalette::Highlight);
        painter->setPen(Qt::NoPen);
        painter->setBrush(blueColor);
        painter->drawRoundedRect(QRect(textX - textMargin,
                                       (d->height - blueSquareWidth) / 2,
                                       blueSquareWidth, blueSquareWidth),
                                 1, 1);
        painter->restore();
    }

    // 绘制文本
    if (isChecked()) {
        color = pal.color(QPalette::Active, QPalette::Text);
        QPen tPen = painter->pen();
        tPen.setColor(color);
        painter->setPen(tPen);
    } else {
        color = pal.color(QPalette::Inactive, QPalette::Text);
        QPen tPen = painter->pen();
        tPen.setColor(color);
        painter->setPen(tPen);
    }
    painter->drawText(textX, (d->height - fm.height()) / 2,
                      fm.horizontalAdvance(str), fm.height(),
                      0, str);

    // 绘制边框线
    painter->save();
    QColor lineColor = DGuiApplicationHelper::instance()->themeType() == DGuiApplicationHelper::DarkType
            ? QColor(255, 255, 255, 20)   // 深色模式：白色 8%
            : QColor(0, 0, 0, 20);   // 浅色模式为8% 的黑色

    pen.setColor(lineColor);
    painter->setPen(pen);

    int y = static_cast<int>(boundingRect().height());
    int x = static_cast<int>(boundingRect().width());

    painter->drawLine(QPointF(x - 1, 0), QPointF(x - 1, y));

    if (!isChecked()) {
        painter->drawLine(QPointF(0, y - 1), QPointF(x - 2, y - 1));
    }
    painter->restore();
    QPalette::ColorGroup cp = isChecked() || d->hovered ? QPalette::Active : QPalette::Inactive;
    pen.setColor(pal.color(cp, QPalette::WindowText));
    painter->setPen(pen);

    // 绘制关闭按钮
    if (d->hovered && d->showCloseButton) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        d->closeButtonRect = QRect(d->width - buttonSize - buttonMargin,
                                   (d->height - buttonSize) / 2,
                                   buttonSize,
                                   buttonSize);

        QColor buttonColor = d->closeButtonHovered ? pal.color(QPalette::Highlight) : pal.color(QPalette::Text);
        painter->setPen(QPen(buttonColor, 1.5));

        painter->drawLine(d->closeButtonRect.topLeft() + QPoint(4, 4), d->closeButtonRect.bottomRight() + QPoint(-4, -4));
        painter->drawLine(d->closeButtonRect.topRight() + QPoint(-4, 4), d->closeButtonRect.bottomLeft() + QPoint(4, -4));

        painter->restore();
    }
}

void Tab::setShowCloseButton(bool show)
{
    if (d->showCloseButton != show) {
        d->showCloseButton = show;
        update();
    }
}

void Tab::setUniqueId(const QString &id)
{
    d->uniqueId = id;
}

QString Tab::uniqueId() const
{
    return d->uniqueId;
}

void Tab::onFileRootUrlChanged(const QUrl &url)
{
    setCurrentUrl(url);
}

void Tab::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (d->dragOutSide) {
        d->pressed = false;
        setZValue(1);
        QGraphicsObject::mouseReleaseEvent(event);

        d->dragOutSide = false;
        d->isDragging = false;
        return;
    }

    d->pressed = false;
    setZValue(1);
    d->isDragging = false;
    d->borderLeft = false;
    update();
    emit draggingFinished();

    QGraphicsObject::mouseReleaseEvent(event);
}

void Tab::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!d->canDrag)
        return QGraphicsObject::mouseMoveEvent(event);

    if (d->isDragging && !d->borderLeft) {
        d->borderLeft = true;
        update();
    }

    if (event->pos().y() < -d->height || event->pos().y() > d->height * 2) {
        if (!d->dragOutSide) {
            d->dragOutSide = true;
            update();
            emit aboutToNewWindow(this);
            emit draggingFinished();
            d->dragObject = new QDrag(this);
            QMimeData *mimeData = new QMimeData;
            int radius = 20;

            const QPixmap &pixmap = toPixmap(true);
            QImage image = Dtk::Widget::dropShadow(pixmap, radius, QColor(0, 0, 0, static_cast<int>(0.2 * 255)));
            QPainter pa(&image);

            pa.drawPixmap(radius, radius, pixmap);

            d->dragObject->setPixmap(QPixmap::fromImage(image));
            d->dragObject->setMimeData(mimeData);
            d->dragObject->setHotSpot(QPoint(150 + radius, 12 + radius));
            d->dragObject->exec();
            d->dragObject->deleteLater();
            d->pressed = false;

            emit requestNewWindow(d->url);
        }
    }

    if (d->dragOutSide) {
        QGraphicsObject::mouseMoveEvent(event);
        return;
    }

    if (pos().x() == 0.0
        && static_cast<int>(pos().x()) == static_cast<int>(scene()->width() - d->width)) {
        QGraphicsObject::mouseMoveEvent(event);
        return;
    }
    setPos(x() + event->pos().x() - event->lastPos().x(), 0);
    emit draggingStarted();
    d->isDragging = true;
    if (pos().x() < 0)
        setX(0);
    else if (pos().x() > scene()->width() - d->width)
        setX(scene()->width() - d->width);

    if (pos().x() > d->originPos.x() + d->width / 2) {
        emit moveNext(this);
        d->originPos.setX(d->originPos.x() + d->width);
    } else if (pos().x() < d->originPos.x() - d->width / 2) {
        emit movePrevius(this);
        d->originPos.setX(d->originPos.x() - d->width);
    }

    QGraphicsObject::mouseMoveEvent(event);
}

void Tab::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (d->hovered && d->showCloseButton && d->closeButtonRect.contains(event->pos().toPoint())) {
            emit closeRequested();
            return;
        }

        emit clicked();

        d->pressed = true;
        d->originPos = pos();
        setZValue(3);
    }
    QGraphicsObject::mousePressEvent(event);
}

void Tab::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    d->hovered = true;
    QGraphicsObject::hoverEnterEvent(event);
}

void Tab::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    bool wasHovered = d->closeButtonHovered;
    d->closeButtonHovered = d->closeButtonRect.contains(event->pos().toPoint());
    if (wasHovered != d->closeButtonHovered) {
        update(d->closeButtonRect);
    }
    QGraphicsObject::hoverMoveEvent(event);
}

void Tab::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    d->hovered = false;
    d->pressed = false;
    QGraphicsObject::hoverLeaveEvent(event);
}

QPixmap Tab::toPixmap(bool drawBorder) const
{
    QImage img(300, d->height, QImage::Format_ARGB32);
    img.fill(Qt::white);
    QPainter painter(&img);
    QPen pen;
    QColor color(Qt::yellow);
    pen.setStyle(Qt::SolidLine);
    pen.setWidth(1);

    // draw text
    QFont font;
    font.setPixelSize(12);
    painter.setFont(font);
    QFontMetrics fm(font);
    QString tabName = d->tabAlias.isEmpty() ? d->tabText : d->tabAlias;
    QString str = fm.elidedText(tabName, Qt::ElideRight, 300 - 10);

    // draw backgound
    color.setNamedColor("#FFFFFF");
    painter.fillRect(boundingRect(), color);
    color.setNamedColor("#303030");
    pen.setColor(color);
    painter.setPen(pen);
    painter.drawText((300 - fm.horizontalAdvance(str)) / 2, (d->height - fm.height()) / 2,
                     fm.horizontalAdvance(str), fm.height(), 0, str);

    if (drawBorder) {
        QPainterPath path;
        path.addRect(0, 0, 300 - 1, d->height - 1);
        color.setRgb(0, 0, 0, static_cast<int>(0.1 * 255));
        pen.setColor(color);
        painter.setPen(pen);
        painter.drawPath(path);
    }

    return QPixmap::fromImage(img);
}

QString Tab::getDisplayNameByUrl(const QUrl &url) const
{
    if (UrlRoute::isRootUrl(url))
        return UrlRoute::rootDisplayName(url.scheme());

    if (SystemPathUtil::instance()->isSystemPath(url.path()))
        return SystemPathUtil::instance()->systemPathDisplayNameByPath(url.path());

    auto info = InfoFactory::create<FileInfo>(url);
    if (info) {
        QString name = info->displayOf(DisPlayInfoType::kFileDisplayName);
        if (!name.isEmpty())
            return name;

        name = info->nameOf(DFMBASE_NAMESPACE::FileInfo::FileNameInfoType::kFileName);
        if (!name.isEmpty())
            return name;
    }

    return url.fileName();
}
