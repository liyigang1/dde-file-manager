// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "iconcachemanager.h"

#include <dtkwidget_global.h>
#include <DGuiApplicationHelper>
#include <DApplication>

#include <QApplication>
#include <QPixmapCache>
#include <QTimer>
#include <QDebug>

DFMBASE_USE_NAMESPACE
DGUI_USE_NAMESPACE
DWIDGET_USE_NAMESPACE

static QTimer *clearTimer()
{
    static QTimer *timer = nullptr;
    if (!timer) {
        timer = new QTimer(qApp);
        timer->setSingleShot(true);
        timer->setInterval(0);   // 下一个事件循环执行
    }
    return timer;
}

void IconCacheManager::initialize()
{
    // 监听 DTK 主题类型变化（深色/浅色切换）
    QObject::connect(DGuiApplicationHelper::instance(),
                     &DGuiApplicationHelper::themeTypeChanged,
                     &IconCacheManager::clear);

    // 监听 Qt 调色板变化（兼容非 DTK 场景，如只改强调色不改主题类型）
    QObject::connect(qApp, &QApplication::paletteChanged,
                     &IconCacheManager::clear);

    QObject::connect(qApp, &DApplication::iconThemeChanged, &IconCacheManager::clear);

    qCDebug(logDFMBase) << "[IconCache] IconCacheManager initialized";
}

QPixmap IconCacheManager::getPixmap(const QString &iconName,
                                    const QSize &size,
                                    QIcon::Mode mode,
                                    QIcon::State state)
{
    // C++11 线程安全的懒初始化：首次调用时自动建立主题监听连接
    static const bool s_initialized = []() {
        initialize();
        return true;
    }();
    Q_UNUSED(s_initialized)

    if (iconName.isEmpty() || !size.isValid() || size.width() <= 0 || size.height() <= 0)
        return QPixmap();

    const QString key = makeCacheKey(iconName, size, mode, state);

    // 尝试从 QPixmapCache 命中
    QPixmap px;
    if (QPixmapCache::find(key, &px))
        return px;

    // 未命中：从主题加载
    QIcon icon = QIcon::fromTheme(iconName);

    // 图标缓存未命中时，回退到"unknown"默认图标避免渲染空白
    if (icon.isNull())
        icon = QIcon::fromTheme("unknown");

    if (icon.isNull())
        return QPixmap();

    px = icon.pixmap(size, mode, state);
    if (!px.isNull())
        QPixmapCache::insert(key, px);

    return px;
}

void IconCacheManager::clear()
{
    // 去重：QTimer::singleShot(0) 合并多次连续调用
    // 如果上一个定时器尚未触发，start() 会重置它
    clearTimer()->start();

    // 连接 timeout 信号（只需连接一次）
    static bool connected = false;
    if (!connected) {
        connected = true;
        QObject::connect(clearTimer(), &QTimer::timeout, []() {
            QPixmapCache::clear();
            qCDebug(logDFMBase) << "[IconCache] Pixmap cache cleared due to theme change";
        });
    }
}

QString IconCacheManager::makeCacheKey(const QString &iconName,
                                       const QSize &size,
                                       QIcon::Mode mode,
                                       QIcon::State state)
{
    // Key 格式: "dfm:icon:{name}:{width}x{height}:{mode}:{state}"
    // DPR 不参与 key —— 主题变化时会 clear()，DPR 变化由 clear() 覆盖
    return QString("dfm:icon:%1:%2x%3:%4:%5")
        .arg(iconName)
        .arg(size.width())
        .arg(size.height())
        .arg(static_cast<int>(mode))
        .arg(static_cast<int>(state));
}
