// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"

#include <dfm-base/base/configs/dconfig/dconfigmanager.h>
#include <dfm-base/utils/loggerrules.h>

#include <dfm-framework/dpf.h>

#include <DApplication>
#include <DSysInfo>

#include <QDebug>
#include <QDir>
#include <QSocketNotifier>

#include <signal.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(logAppServer, "org.deepin.dde.filemanager.server")

static constexpr char kServerInterface[] { "org.deepin.plugin.server" };
static constexpr char kPluginCore[] { "serverplugin-core" };
static constexpr char kLibCore[] { "libserverplugin-core.so" };
static int kSigtermFlag = 0;

// Self-pipe trick: fd[0]=read end (QSocketNotifier), fd[1]=write end (signal handler)
static int g_sigTermPipe[2] { -1, -1 };
DFMBASE_USE_NAMESPACE

#ifdef DFM_ORGANIZATION_NAME
#    define ORGANIZATION_NAME DFM_ORGANIZATION_NAME
#else
#    define ORGANIZATION_NAME "deepin"
#endif

DWIDGET_USE_NAMESPACE
DCORE_USE_NAMESPACE

static void initLog()
{
#ifdef DTKCORE_CLASS_DConfigFile
    LoggerRules::instance().initLoggerRules();
#endif
    dpfLogManager->applySuggestedLogSettings();
}

static bool pluginsLoad()
{
    QString msg;
    if (!DConfigManager::instance()->addConfig(kPluginsDConfName, &msg))
        qCWarning(logAppServer) << "Load plugins but dconfig failed: " << msg;

    QStringList pluginsDirs;
#ifdef QT_DEBUG
    const QString &pluginsDir { DFM_BUILD_PLUGIN_DIR };
    qCInfo(logAppServer) << QString("Load plugins path : %1").arg(pluginsDir);
    pluginsDirs.push_back(pluginsDir + "/server");
    pluginsDirs.push_back(pluginsDir);
#else
    pluginsDirs << QString(DFM_PLUGIN_FILEMANAGER_CORE_DIR)
                << QString(DFM_PLUGIN_SERVER_EDGE_DIR);
#endif
    qCInfo(logAppServer) << "Using plugins dir:" << pluginsDirs;
    QStringList blackNames { DConfigManager::instance()->value(kPluginsDConfName, "server.blackList").toStringList() };
    DPF_NAMESPACE::LifeCycle::initialize({ kServerInterface }, pluginsDirs, blackNames);

    qCInfo(logAppServer) << "Depend library paths:" << QCoreApplication::libraryPaths();
    qCInfo(logAppServer) << "Load plugin paths: " << dpf::LifeCycle::pluginPaths();

    // read all plugins in setting paths
    if (!DPF_NAMESPACE::LifeCycle::readPlugins())
        return false;

    // We should make sure that the core plugin is loaded first
    auto corePlugin = DPF_NAMESPACE::LifeCycle::pluginMetaObj(kPluginCore);
    if (corePlugin.isNull())
        return false;
    if (!corePlugin->fileName().contains(kLibCore)) {
        qCWarning(logAppServer) << corePlugin->fileName() << "is not" << kLibCore;
        return false;
    }
    if (!DPF_NAMESPACE::LifeCycle::loadPlugin(corePlugin))
        return false;

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

[[noreturn]] static void handleSIGABRT(int sig)
{
    qCCritical(logAppServer) << "break with !SIGABRT! " << sig;
    // WORKAROUND: cannot receive SIGTERM when shutdown or reboot
    // see: bug-228373
    ::_exit(1);
}

DWIDGET_USE_NAMESPACE

int main(int argc, char *argv[])
{
    initLog();
    DApplication a(argc, argv);
    a.setOrganizationName(ORGANIZATION_NAME);
    {
        // load translation
        QString appName = a.applicationName();
        a.setApplicationName("dde-file-manager");
        a.loadTranslator();
        a.setApplicationName(appName);
    }

    // Set up self-pipe so the signal handler can safely wake the main event loop
    if (::pipe(g_sigTermPipe) != 0) {
        qCWarning(logAppServer) << "main: Failed to create SIGTERM self-pipe";
    } else {
        auto *sigTermNotifier = new QSocketNotifier(g_sigTermPipe[0], QSocketNotifier::Read, &a);
        QObject::connect(sigTermNotifier, &QSocketNotifier::activated, &a, [&a]() {
            (void)::read(g_sigTermPipe[0], &kSigtermFlag, sizeof(kSigtermFlag));
            qCInfo(logAppServer) << "main: SIGTERM received via self-pipe, quitting main event loop, SIGTERM = " << kSigtermFlag ;
            a.quit();
        });
    }

    signal(SIGTERM, handleSIGTERM);
    signal(SIGABRT, handleSIGABRT);

    DPF_NAMESPACE::backtrace::installStackTraceHandler();

    if (!pluginsLoad()) {
        qCCritical(logAppServer) << "Load pugin failed!";
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
    return ret;
}
