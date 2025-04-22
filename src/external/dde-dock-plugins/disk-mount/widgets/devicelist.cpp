// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "devicelist.h"
#include "deviceitem.h"
#include "device/dockitemdatamanager.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QScrollBar>
#include <QTouchEvent>
#include <QtMath>

#include <DGuiApplicationHelper>

inline constexpr int kTouchSlop { 10 };

Q_DECLARE_LOGGING_CATEGORY(logAppDock)

using namespace Dtk::Gui;

DeviceList::DeviceList(QWidget *parent)
    : QScrollArea(parent)
{
    this->setObjectName("DiskControlWidget-QScrollArea");
    initUI();
    initConnect();

    viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
    scroller = QScroller::scroller(viewport());
    QScroller::grabGesture(viewport(), QScroller::TouchGesture);
    viewport()->installEventFilter(this);
}

void DeviceList::addDevice(const DockItemData &item)
{
    if (deviceItems.contains(item.id))
        removeDevice(item.id);

    auto devItem = new DeviceItem(item, this);
    connect(devItem, &DeviceItem::requestEject,
            this, &DeviceList::ejectDevice);
    deviceItems.insert(item.id, devItem);
    sortKeys.insert(item.id, item.sortKey);
    QStringList order = sortKeys.values();
    qSort(order);
    deviceLay->insertWidget(order.indexOf(item.sortKey), devItem);
    qCInfo(logAppDock) << "added item:" << item.id << devItem;
    updateHeight();
}

void DeviceList::removeDevice(const QString &id)
{
    auto wgt = deviceItems.value(id, nullptr);
    if (wgt) {
        qCInfo(logAppDock) << "removed item:" << id << wgt;
        deviceLay->removeWidget(wgt);
        delete wgt;
        wgt = nullptr;
        deviceItems.remove(id);
        sortKeys.remove(id);
        updateHeight();
    }
}

void DeviceList::ejectDevice(const QString &id)
{
    qCInfo(logAppDock) << "about to eject" << id;
    DockItemDataManager::instance()->ejectDevice(id);
}

bool DeviceList::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == viewport()) {
        switch (event->type()) {
        case QEvent::TouchBegin: {
            QTouchEvent *touchEvent = static_cast<QTouchEvent *>(event);
            if (touchEvent->touchPoints().count() == 1) {
                touchStartPoint = touchEvent->touchPoints().first().pos();
                touchMoved = false;
                return false;
            }
            break;
        }
        case QEvent::TouchUpdate: {
            QTouchEvent *touchEvent = static_cast<QTouchEvent *>(event);
            if (touchEvent->touchPoints().count() == 1) {
                const QPointF currentPos = touchEvent->touchPoints().first().pos();
                const qreal distance = QLineF(touchStartPoint, currentPos).length();
                if (distance > kTouchSlop) {
                    touchMoved = true;
                    event->accept();
                    return true;
                }
            }
            break;
        }
        case QEvent::TouchEnd: {
            if (!touchMoved) {
                return false;
            }
            touchMoved = false;
            break;
        }
        default:
            break;
        }
    }
    return QScrollArea::eventFilter(watched, event);
}

void DeviceList::initUI()
{
    deviceLay = new QVBoxLayout();
    deviceLay->setMargin(0);
    deviceLay->setSpacing(0);

    QVBoxLayout *mainLay = new QVBoxLayout();
    mainLay->setMargin(0);
    mainLay->setSpacing(0);
    mainLay->setSizeConstraint(QLayout::SetFixedSize);

    mainLay->addWidget(createHeader());
    mainLay->addLayout(deviceLay);

    QWidget *content = new QWidget(this);
    content->setLayout(mainLay);
    setWidget(content);

    setFixedWidth(kDockPluginWidth);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    verticalScrollBar()->setSingleStep(7);
    viewport()->setAutoFillBackground(false);
    content->setAutoFillBackground(false);
    setWidgetResizable(true);
    setMaximumHeight(420);
}

void DeviceList::initConnect()
{
    manager = DockItemDataManager::instance();
    connect(manager, &DockItemDataManager::mountAdded,
            this, &DeviceList::addDevice);
    connect(manager, &DockItemDataManager::mountRemoved,
            this, &DeviceList::removeDevice);
    connect(manager, &DockItemDataManager::usageUpdated,
            this, [this](auto id, auto used) {
                auto item = deviceItems.value(id, nullptr);
                auto devItem = dynamic_cast<DeviceItem *>(item);
                if (devItem) devItem->updateUsage(used);
            });
    manager->initialize();
}

void DeviceList::updateHeight()
{
    int contentHeight = 50 + kDeviceItemHeight * deviceItems.count();
    if (contentHeight > 420)
        contentHeight = 420;
    resize(width(), contentHeight);
}

QWidget *DeviceList::createHeader()
{
    QWidget *header = new QWidget(this);
    header->setFixedWidth(kDockPluginWidth);
    QVBoxLayout *lay = new QVBoxLayout();
    lay->setSpacing(0);
    lay->setContentsMargins(20, 9, 0, 8);

    QVBoxLayout *headerLay = new QVBoxLayout();
    headerLay->setContentsMargins(0, 0, 0, 0);
    headerLay->setSpacing(0);
    header->setLayout(headerLay);

    QLabel *title = new QLabel(tr("Disks"), this);
    lay->addWidget(title);

    auto line = DeviceItem::createSeparateLine(1);
    line->setParent(this);
    headerLay->addLayout(lay);
    headerLay->addWidget(line);

    QFont f = title->font();
    f.setPixelSize(20);
    f.setWeight(QFont::Medium);
    title->setFont(f);

    auto setTextColor = [title](DGuiApplicationHelper::ColorType) {
        QPalette pal = title->palette();
        QColor color = Qt::white;
        if (DGuiApplicationHelper::instance()->themeType() == DGuiApplicationHelper::LightType)
            color = Qt::black;
        // pal.setColor(QPalette::WindowText, color);
        title->setPalette(pal);
    };

    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged,
            title, [=](auto type) { setTextColor(type); });
    setTextColor(DGuiApplicationHelper::instance()->themeType());
    return header;
}
