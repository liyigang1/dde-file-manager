// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"   //cmake

#include <DApplication>
#include <QDir>
#include <QTextCodec>
#include <QIcon>
#include <QSocketNotifier>

#include <dfm-base/dfm_plugin_defines.h>
#include <dfm-base/base/configs/dconfig/dconfigmanager.h>
#include <dfm-base/utils/loggerrules.h>

#include <dfm-framework/dpf.h>

#include <signal.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(logAppDialogX11, "org.deepin.dde.filemanager.filedialog-x11")

DGUI_USE_NAMESPACE
DWIDGET_USE_NAMESPACE
DFMBASE_USE_NAMESPACE

#ifdef DFM_ORGANIZATION_NAME
#    define ORGANIZATION_NAME DFM_ORGANIZATION_NAME
#else
#    define ORGANIZATION_NAME "deepin"
#endif

static constexpr char kDialogPluginInterface[] { "org.deepin.plugin.filedialog" };
static constexpr char kFmPluginInterface[] { "org.deepin.plugin.filemanager" };
static constexpr char kCommonPluginInterface[] { "org.deepin.plugin.common" };

static constexpr char kDialogCorePluginName[] { "filedialogplugin-core" };
static constexpr char kDialogCoreLibName[] { "libfiledialogplugin-core.so" };
static constexpr char kDFMCorePluginName[] { "dfmplugin-core" };
static constexpr char kDFMCoreLibName[] { "libdfmplugin-core.so" };
static int kSigtermFlag = 0;

// Self-pipe trick: fd[0]=read end (QSocketNotifier), fd[1]=write end (signal handler)
static int g_sigTermPipe[2] { -1, -1 };

static void initLog()
{
#ifdef DTKCORE_CLASS_DConfigFile
    LoggerRules::instance().initLoggerRules();
#endif
    dpfLogManager->applySuggestedLogSettings();
}

static void initEnv()
{
    // Note: x11 flag!!!
    qputenv("QT_QPA_PLATFORM", "xcb");

    // for qt5platform-plugins load DPlatformIntegration or DPlatformIntegrationParent
    if (qEnvironmentVariableIsEmpty("XDG_CURRENT_DESKTOP")) {
        qputenv("XDG_CURRENT_DESKTOP", "Deepin");
    }

    if (qEnvironmentVariable("CLUTTER_IM_MODULE") == QStringLiteral("fcitx")) {
        setenv("QT_IM_MODULE", "fcitx", 1);
    }
}

static bool singlePluginLoad(const QString &pluginName, const QString &libName)
{
    auto plugin = DPF_NAMESPACE::LifeCycle::pluginMetaObj(pluginName);
    if (plugin.isNull())
        return false;
    if (!plugin->fileName().contains(libName))
        return false;
    if (!DPF_NAMESPACE::LifeCycle::loadPlugin(plugin))
        return false;

    return true;
}

static bool lazyLoadFilter(const QString &name)
{
    static const auto &kAllNames {
        DFMBASE_NAMESPACE::Plugins::Utils::filemanagerCorePlugins()
    };

    if (!kAllNames.contains(name) && (name != kDialogCorePluginName))
        return true;
    return false;
}

static bool blackListFilter(const QString &name)
{
    static const auto &kBlackNames { DConfigManager::instance()->value(kPluginsDConfName, "filedialog.blackList").toStringList() };
    if (kBlackNames.contains(name))
        return true;
    static const auto &kAllNames {
        DFMBASE_NAMESPACE::Plugins::Utils::filemanagerAllPlugins()
    };

    {
        // FIXME(zhangsheng) FIXME(xust): find another way to solve this.
        static const QStringList tmpWhiteList {
            "dfmplugin-disk-encrypt", "dfmplugin-encrypt-manager"
        };
        if (tmpWhiteList.contains(name))
            return false;
    }

    if (!kAllNames.contains(name) && (name != kDialogCorePluginName))
        return true;
    return false;
}

static bool pluginsLoad()
{
    QString msg;
    if (!DConfigManager::instance()->addConfig(kPluginsDConfName, &msg))
        qCWarning(logAppDialogX11) << "Load plugins but dconfig failed: " << msg;

    QStringList pluginsDirs;
#ifdef QT_DEBUG
    const QString &pluginsDir { DFM_BUILD_PLUGIN_DIR };
    qCInfo(logAppDialogX11) << QString("Load plugins path : %1").arg(pluginsDir);
    pluginsDirs.push_back(pluginsDir + "/filemanager");
    pluginsDirs.push_back(pluginsDir + "/common");
    pluginsDirs.push_back(pluginsDir);
#else
    pluginsDirs << QString(DFM_PLUGIN_COMMON_CORE_DIR)
                << QString(DFM_PLUGIN_FILEMANAGER_CORE_DIR)
                << QString(DFM_PLUGIN_COMMON_EDGE_DIR)
                << QString(DFM_PLUGIN_FILEMANAGER_EDGE_DIR);
#endif

    qCInfo(logAppDialogX11) << "Using plugins dir:" << pluginsDirs;
    DPF_NAMESPACE::LifeCycle::initialize({ kDialogPluginInterface,
                                           kFmPluginInterface,
                                           kCommonPluginInterface },
                                         pluginsDirs);
    DPF_NAMESPACE::LifeCycle::setLazyloadFilter(lazyLoadFilter);
    DPF_NAMESPACE::LifeCycle::setBlackListFilter(blackListFilter);

    qCInfo(logAppDialogX11) << "Depend library paths:" << DApplication::libraryPaths();
    qCInfo(logAppDialogX11) << "Load plugin paths: " << dpf::LifeCycle::pluginPaths();

    // read all plugins in setting paths
    if (!DPF_NAMESPACE::LifeCycle::readPlugins())
        return false;

    // We should make sure that the core plugin is loaded first
    if (!singlePluginLoad(kDialogCorePluginName, kDialogCoreLibName)) {
        qCWarning(logAppDialogX11) << "Load" << kDialogCorePluginName << "failed";
        return false;
    }
    if (!singlePluginLoad(kDFMCorePluginName, kDFMCoreLibName)) {
        qCWarning(logAppDialogX11) << "Load" << kDFMCorePluginName << "failed";
        return false;
    }

    // load plugins without core
    if (!DPF_NAMESPACE::LifeCycle::loadPlugins())
        return false;

    return true;
}

static void handleSIGTERM(int sig)
{
    // Only async-signal-safe operations are allowed here.
    // write() is async-signal-safe; qApp->quit() is NOT, so we use self-pipe trick
    // to delegate the actual quit() call to the main event loop via QSocketNotifier.
    (void)::write(g_sigTermPipe[1], &sig, sizeof(sig));
}

int main(int argc, char *argv[])
{
    initEnv();
    initLog();

    // Fixed the locale codec to utf-8
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("utf-8"));

    DApplication a(argc, argv);
    a.setOrganizationName(ORGANIZATION_NAME);
    a.setAttribute(Qt::AA_UseHighDpiPixmaps);
    a.setQuitOnLastWindowClosed(false);
    a.setProperty("GTK", true);   // see: FileDialogHandle::winId()
    a.setWindowIcon(QIcon::fromTheme("dde-file-manager"));

    {
        // load translation
        auto appName = a.applicationName();
        a.setApplicationName("dde-file-manager");
        a.loadTranslator();
        a.setApplicationName(appName);
    }

    // Set up self-pipe so the signal handler can safely wake the main event loop
    if (::pipe(g_sigTermPipe) != 0) {
        qCWarning(logAppDialogX11) << "main: Failed to create SIGTERM self-pipe";
    } else {
        auto *sigTermNotifier = new QSocketNotifier(g_sigTermPipe[0], QSocketNotifier::Read, &a);
        QObject::connect(sigTermNotifier, &QSocketNotifier::activated, &a, [&a]() {
            (void)::read(g_sigTermPipe[0], &kSigtermFlag, sizeof(kSigtermFlag));
            qCInfo(logAppDialogX11) << "main: SIGTERM received via self-pipe, quitting main event loop, SIGTERM = " << kSigtermFlag ;
            a.quit();
        });
    }

    signal(SIGTERM, handleSIGTERM);

    DPF_NAMESPACE::backtrace::installStackTraceHandler();

    if (!pluginsLoad()) {
        qCCritical(logAppDialogX11) << "Load pugin failed!";
        abort();
    }

    int ret { a.exec() };
    // Close self-pipe fds to release kernel resources
    if (g_sigTermPipe[0] != -1) {
       ::close(g_sigTermPipe[0]);
       ::close(g_sigTermPipe[1]);
       g_sigTermPipe[0] = g_sigTermPipe[1] = -1;
    }
    DPF_NAMESPACE::LifeCycle::shutdownPlugins();
    if (kSigtermFlag != 0) {
        qCWarning(logAppDialogX11) << "Exit app by SIGTERM, reuturn: " << ret << kSigtermFlag;
        _Exit(ret);
    }

    return ret;
}
