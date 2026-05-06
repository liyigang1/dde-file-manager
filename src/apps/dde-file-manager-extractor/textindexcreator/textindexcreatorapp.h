// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TEXTINDEXCREATORAPP_H
#define TEXTINDEXCREATORAPP_H

#include "core/indexruntime.h"
#include "textindexcreator/extractor/pluginloader.h"
#include "workerpipe.h"

#include <QObject>
#include <QSharedPointer>
#include <QTimer>

EXTRACTOR_PLUGIN_BEGIN_NAMESPACE

/**
 * @brief TextIndexCreatorApp is the main application class for the extractor subprocess.
 *
 * It manages the plugin loader and handles extraction requests from stdin.
 */
class TextIndexCreatorApp : public QObject
{
    Q_OBJECT

public:
    explicit TextIndexCreatorApp(QObject *parent = nullptr);
    ~TextIndexCreatorApp() override;

    /**
     * @brief Initialize the extractor application.
     *
     * @param pluginPath Path to the plugin directory
     * @return true if initialization succeeded
     */
    bool initialize(const QString &pluginPath);

    /**
     * @brief Run the main event loop.
     *
     * Processes extraction requests until stdin is closed.
     */
    void run();

private:
    static constexpr int kIdleTimeoutMs = 10 * 60 * 1000;

    void resetIdleTimer();

    QSharedPointer<PluginLoader> m_pluginLoader;
    QScopedPointer<TEXTINDEX_CREATOR_IPC_NAMESPACE::WorkerPipe> m_workerPipe;
    QTimer *m_idleTimer = nullptr;
    TEXTINDEX_CREATOR_NAMESPACE::IndexRuntime *runtime { nullptr };
};

EXTRACTOR_PLUGIN_END_NAMESPACE

#endif   // TEXTINDEXCREATORAPP_H
