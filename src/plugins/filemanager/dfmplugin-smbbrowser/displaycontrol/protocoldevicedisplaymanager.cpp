// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "protocoldevicedisplaymanager.h"
#include "protocoldevicedisplaymanager_p.h"
#include "displaycontrol/datahelper/virtualentrydbhandler.h"
#include "displaycontrol/info/protocolvirtualentryentity.h"
#include "displaycontrol/menu/virtualentrymenuscene.h"
#include "displaycontrol/utilities/protocoldisplayutilities.h"

#include "plugins/common/core/dfmplugin-menu/menu_eventinterface_helper.h"

#include <dfm-base/base/configs/dconfig/dconfigmanager.h>
#include <dfm-base/base/application/application.h>
#include <dfm-base/base/application/settings.h>
#include <dfm-base/base/device/deviceproxymanager.h>
#include <dfm-base/base/device/deviceutils.h>

#include <dfm-framework/event/event.h>

#include <QMenu>

DPSMBBROWSER_USE_NAMESPACE
DFMBASE_USE_NAMESPACE
using namespace computer_sidebar_event_calls;
using namespace protocol_display_utilities;
using namespace ui_ventry_calls;

Q_DECLARE_METATYPE(QList<QUrl> *)

namespace dfm_dconfig {
static constexpr char kShowOffline[] { "dfm.samba.permanent" };
}   // namespace dfm_dconfig

namespace dfm_json_config {
static constexpr char kSmbAggregation[] { "MergeTheEntriesOfSambaSharedFolders" };
static constexpr char kGenericAttribute[] { "GenericAttribute" };
}   // namespace dfm_json_config

namespace plugin_events {
static constexpr char kComputerMenu[] { "ComputerMenu" };

static constexpr char kComputerEventNS[] { "dfmplugin_computer" };
static constexpr char kCptHookAdd[] { "hook_View_ItemFilterOnAdd" };
static constexpr char kCptHookRemove[] { "hook_View_ItemFilterOnRemove" };
static constexpr char kCptHookListFilter[] { "hook_View_ItemListFilter" };
}   // namespace plugin_events

ProtocolDeviceDisplayManager::ProtocolDeviceDisplayManager(QObject *parent)
    : QObject { parent }, d(new ProtocolDeviceDisplayManagerPrivate(this))
{
    fmDebug() << "init";
    d->init();
    fmDebug() << "init finished";
}

ProtocolDeviceDisplayManager::~ProtocolDeviceDisplayManager()
{
}

ProtocolDeviceDisplayManager *ProtocolDeviceDisplayManager::instance()
{
    // 静态变量再堆上分配，最后析构不会有顺序问题
    static ProtocolDeviceDisplayManager *ins = new ProtocolDeviceDisplayManager;
    return ins;
}

SmbDisplayMode ProtocolDeviceDisplayManager::displayMode() const
{
    return d->displayMode;
}

bool ProtocolDeviceDisplayManager::isShowOfflineItem() const
{
    return d->showOffline;
}

bool ProtocolDeviceDisplayManager::hookItemInsert(const QUrl &entryUrl)
{
    if (!d->isSupportVEntry(entryUrl))
        return false;

    fmDebug() << entryUrl << "about to be inserted";

    // 只有 samba 才区分聚合与分离，其他协议都只显示离线入口
    if (displayMode() == kAggregation && DeviceUtils::isSamba(entryUrl.path())) {
        fmDebug() << "add aggregation item, ignore seperated item";
        QTimer::singleShot(0, this, [=] { addAggregatedItemForSeperatedOnlineItem(entryUrl); });
        return true;
    }

    return false;
}

bool ProtocolDeviceDisplayManager::hookItemsFilter(QList<QUrl> *entryUrls)
{
    // FTP/SFTP处理：只关注离线显示开关，与显示模式无关
    if (isShowOfflineItem()) {
        QTimer::singleShot(0, this, [=] { addOfflineProtocolItems(); });
    }

    if (displayMode() == kSeperate) {
        if (isShowOfflineItem())
            QTimer::singleShot(0, this, [=] { addSeperatedOfflineItems(); });
        return false;
    }

    d->removeAllSmb(entryUrls);
    QTimer::singleShot(0, this, [=] { addAggregatedItems(); });
    return true;
}

void ProtocolDeviceDisplayManager::onDevMounted(const QString &id, const QString &)
{
    if (!isShowOfflineItem())
        return;

    // 处理FTP/SFTP设备
    if (DeviceUtils::isFtp(QUrl(id)) || DeviceUtils::isSftp(QUrl(id))) {
        const QString &stdPath = getStandardProtocolPath(id);
        const QString &displayName = getDisplayNameOf(id);

        // 移除旧的离线入口，避免重复显示。该场景可能发生于用户切换了编码集进行挂载。
        // 两个编码集会被认为是两个条目，会导致离线入口重复显示。
        // 所以将主机地址相同的 ftp 条目都先移除，再重新添加。
        auto allCached = VirtualEntryDbHandler::instance()->allProtocolIDs();
        for (const auto &cached : allCached) {
            auto cachedUrl = QUrl(cached);
            if (cachedUrl.host() == QUrl(stdPath).host()
                && cachedUrl.scheme() == QUrl(stdPath).scheme()) {
                auto entryUrl = makeVEntryUrl(cached);
                callItemRemove(entryUrl);
            }
        }

        VirtualEntryDbHandler::instance()->saveProtocolData(stdPath, displayName);
        return;
    } else if (DeviceUtils::isSamba(QUrl(id))) {
        // obtain the display name of `id`
        const QString &displayName = getDisplayNameOf(id);
        const QString &stdSmbPath = getStandardProtocolPath(id);
        VirtualEntryDbHandler::instance()->saveAggregatedAndSperated(stdSmbPath, displayName);

        const QUrl &vEntryUrl = makeVEntryUrl(stdSmbPath);
        callItemRemove(vEntryUrl);
    }
}

void ProtocolDeviceDisplayManager::onDevUnmounted(const QString &id)
{
    // 处理FTP/SFTP设备
    if (DeviceUtils::isFtp(QUrl(id)) || DeviceUtils::isSftp(QUrl(id))) {
        if (!isShowOfflineItem())
            return;

        const QString &stdPath = getStandardProtocolPath(id);
        if (!VirtualEntryDbHandler::instance()->hasOfflineEntry(stdPath))
            return;

        const QUrl &vEntryUrl = makeVEntryUrl(stdPath);
        callItemAdd(vEntryUrl);
        return;
    }

    if (!DeviceUtils::isSamba(QUrl(id)))
        return;

    if (displayMode() == SmbDisplayMode::kSeperate && isShowOfflineItem()) {
        const QString &stdSmbPath = getStandardProtocolPath(id);
        // persistent data will be removed if "forget password" is triggered.
        // in this case, do not show the virtual entry.
        if (!VirtualEntryDbHandler::instance()->hasOfflineEntry(stdSmbPath))
            return;
        const QUrl &vEntryUrl = makeVEntryUrl(stdSmbPath);
        callItemAdd(vEntryUrl);
    } else {
        // only remove aggregated entry when there has no mounted share of the host.
        const QString &stdRemovedSmb = getStandardProtocolPath(id);
        QString host = QUrl(stdRemovedSmb).host();
        QString removedHost = QString("smb://") + host;

        const auto &allMountedStdSmb = getStandardSmbPaths(getMountedSmb());
        bool hasMountedOfHost = std::any_of(allMountedStdSmb.cbegin(), allMountedStdSmb.cend(),
                                            [=](const QString &smb) { return smb.startsWith(removedHost); });
        if (hasMountedOfHost) {
            return;
        } else {
            secret_utils::forgetPasswordInSession(host);
        }

        if (isShowOfflineItem())
            return;
        QUrl entryUrl = makeVEntryUrl(removedHost);
        callItemRemove(entryUrl);
    }
}

void ProtocolDeviceDisplayManager::onDConfigChanged(const QString &g, const QString &k)
{
    using namespace dfm_dconfig;
    if (g == kDefaultCfgPath && k == dfm_dconfig::kShowOffline) {
        d->showOffline = DConfigManager::instance()->value(kDefaultCfgPath, kShowOffline).toBool();
        d->onShowOfflineChanged();
        fmDebug() << "showOffline changed: " << d->showOffline;
    }
}

void ProtocolDeviceDisplayManager::onJsonConfigChanged(const QString &g, const QString &k, const QVariant &v)
{
    using namespace dfm_json_config;
    if (g == kGenericAttribute && k == kSmbAggregation) {
        d->displayMode = v.toBool() ? kAggregation : kSeperate;
        d->onDisplayModeChanged();
        fmDebug() << "displayMode changed: " << d->displayMode;
    }
}

void ProtocolDeviceDisplayManager::onMenuSceneAdded(const QString &scene)
{
    if (scene != plugin_events::kComputerMenu)
        return;
    bool ok = dfmplugin_menu_util::menuSceneBind(VirtualEntryMenuCreator::name(), scene);
    fmInfo() << "bind virtual entry menu to computer: " << ok;
}

void ProtocolDeviceDisplayManagerPrivate::init()
{
    using namespace dfm_json_config;
    using namespace dfm_dconfig;
    showOffline = DConfigManager::instance()->value(kDefaultCfgPath, kShowOffline).toBool();
    displayMode = Application::genericSetting()->value(kGenericAttribute, kSmbAggregation).toBool()
            ? kAggregation
            : kSeperate;

    // watch confgi changes
    q->connect(DConfigManager::instance(), &DConfigManager::valueChanged, q, &ProtocolDeviceDisplayManager::onDConfigChanged);
    q->connect(Application::genericSetting(), &Settings::valueChanged, q, &ProtocolDeviceDisplayManager::onJsonConfigChanged);

    // watch device actions
    q->connect(DevProxyMng, &DeviceProxyManager::protocolDevMounted, q, &ProtocolDeviceDisplayManager::onDevMounted);
    q->connect(DevProxyMng, &DeviceProxyManager::protocolDevUnmounted, q, &ProtocolDeviceDisplayManager::onDevUnmounted);

    // hook computer events
    using namespace plugin_events;
    dpfHookSequence->follow(kComputerEventNS, kCptHookAdd, q, &ProtocolDeviceDisplayManager::hookItemInsert);
    dpfHookSequence->follow(kComputerEventNS, kCptHookListFilter, q, &ProtocolDeviceDisplayManager::hookItemsFilter);

    // regist entity info
    EntryEntityFactor::registCreator<ProtocolVirtualEntryEntity>(kVEntrySuffix);

    // regist menu and bind to computer
    using namespace dfmplugin_menu_util;
    menuSceneRegisterScene(VirtualEntryMenuCreator::name(), new VirtualEntryMenuCreator());

    if (menuSceneContains(kComputerMenu))
        menuSceneBind(VirtualEntryMenuCreator::name(), kComputerMenu);
    else
        dpfSignalDispatcher->subscribe("dfmplugin_menu", "signal_MenuScene_SceneAdded",
                                       q, &ProtocolDeviceDisplayManager::onMenuSceneAdded);
}

void ProtocolDeviceDisplayManagerPrivate::onDisplayModeChanged()
{
    callComputerRefresh();
}

void ProtocolDeviceDisplayManagerPrivate::onShowOfflineChanged()
{
    const QStringList &allMounted = getMountedSmb();
    if (showOffline) {
        std::for_each(allMounted.cbegin(), allMounted.cend(), [=](const QString &devId) {
            const QString &displayName = getDisplayNameOf(devId);
            const QString &stdPath = getStandardProtocolPath(devId);

            QUrl url(stdPath);
            if (url.scheme() == "smb") {
                VirtualEntryDbHandler::instance()->saveAggregatedAndSperated(stdPath, displayName);
            } else if (url.scheme() == "ftp" || url.scheme() == "sftp") {
                VirtualEntryDbHandler::instance()->saveProtocolData(stdPath, displayName);
            }
        });
    } else {
        // 移除FTP/SFTP虚拟条目
        QStringList allCached = VirtualEntryDbHandler::instance()->allProtocolIDs();
        for (const auto &cached : allCached) {
            QUrl url(cached);
            if (url.scheme() == "ftp" || url.scheme() == "sftp") {
                auto entryUrl = makeVEntryUrl(cached);
                callItemRemove(entryUrl);
            }
        }

        // remove all visible virtual entry
        const QStringList &allStdSmb = getStandardSmbPaths(allMounted);
        QStringList allAggregated, allSeperated;
        VirtualEntryDbHandler::instance()->allProtocolIDs(&allAggregated, &allSeperated);

        if (displayMode == SmbDisplayMode::kSeperate) {
            // if in seperated mode, remove all offline item
            std::for_each(allSeperated.cbegin(), allSeperated.cend(), [=](const QString &seperated) {
                auto entryUrl = makeVEntryUrl(seperated);
                callItemRemove(entryUrl);
            });
        } else {
            // else in aggregated mode, remove the entry which is just a virtual entry, no mounted share of the host
            QStringList pureVirtualEntry;
            auto hasMountedShareOf = [&allStdSmb](const QString &host) {
                return std::any_of(allStdSmb.cbegin(), allStdSmb.end(), [&](const QString &mounted) {
                    return mounted.startsWith(host);
                });
            };
            // find orphan host
            std::for_each(allAggregated.cbegin(), allAggregated.cend(), [&](const QString &host) {
                if (!hasMountedShareOf(host))
                    pureVirtualEntry << host;
            });
            std::for_each(pureVirtualEntry.cbegin(), pureVirtualEntry.cend(), [=](const QString &host) {
                auto entryUrl = makeVEntryUrl(host);
                callItemRemove(entryUrl);
            });
        }

        VirtualEntryDbHandler::instance()->clearData();
    }
}

bool ProtocolDeviceDisplayManagerPrivate::isSupportVEntry(const QUrl &entryUrl)
{
    if (!showOffline && displayMode == kSeperate)
        return false;

    QString path = entryUrl.path();
    if (!path.endsWith(kComputerProtocolSuffix))
        return false;

    path.remove("." + QString(kComputerProtocolSuffix));
    return DeviceUtils::isSamba(path) || DeviceUtils::isFtp(path) || DeviceUtils::isSftp(path);
}

bool ProtocolDeviceDisplayManagerPrivate::isSupportVEntry(const QString &devId)
{
    if (!showOffline && displayMode == kSeperate)
        return false;
    if (!DeviceUtils::isSamba(devId))
        return false;
    return true;
}

void ProtocolDeviceDisplayManagerPrivate::removeAllSmb(QList<QUrl> *entryUrls)
{
    Q_ASSERT(entryUrls);
    for (int i = entryUrls->count() - 1; i >= 0; i--) {
        const auto &entryUrl = entryUrls->at(i);
        if (!isSupportVEntry(entryUrl))
            continue;
        if (DeviceUtils::isSamba(entryUrl.path()))
            entryUrls->removeAt(i);
    }
}
