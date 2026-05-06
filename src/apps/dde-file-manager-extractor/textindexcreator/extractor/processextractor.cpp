// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "processextractor.h"

#include <controllerpipe.h>

#include <QFile>

EXTRACTOR_PLUGIN_USE_NAMESPACE
TEXTINDEX_CREATOR_BEGIN_NAMESPACE

class ProcessExtractorPrivate
{
public:
    ProcessExtractorPrivate()
    {
    }

    ~ProcessExtractorPrivate()
    {
    }

    /**
     * @brief Initialize the extractor application.
     *
     * @param pluginPath Path to the plugin directory
     * @return true if initialization succeeded
     */
    bool initialize(const QString &pluginPath);

    PluginLoader m_pluginLoader;
};

ProcessExtractor::ProcessExtractor()
    : d(new ProcessExtractorPrivate())
{
}

ProcessExtractor::~ProcessExtractor()
{
}

IndexExtractionResult ProcessExtractor::extract(const QString &filePath, size_t maxBytes) const
{
    Q_UNUSED(maxBytes);
    fmInfo() << "[ProcessExtractor::extract]: Processing batch of" << filePath << "files";

    IndexExtractionResult re;
    // Check if file exists
    if (!QFile::exists(filePath)) {
        fmWarning() << "[ProcessExtractor::extract]: File does not exist:" << filePath;
        re.error = "File does not exist";
        return re;
    }

    // Find appropriate plugin
    auto plugin = d->m_pluginLoader.findPlugin(filePath);
    if (!plugin) {
        fmWarning() << "[ProcessExtractor::extract]: No plugin can handle file:" << filePath;
        re.error = "No plugin available for this file type";
        return re;
    }

    // Extract content
    const auto &result = plugin->extract(filePath);
    if (!result.has_value()) {
        fmWarning() << "[ProcessExtractor::extract]: Extraction failed for file:" << filePath;
        re.error = "Extraction failed";
        return re;
    }


    fmDebug() << "[ProcessExtractor::extract]: Successfully extracted" << result->size()
              << "bytes from:" << filePath;

    re.success = true;
    re.text = result.value();
    return re;
}

bool ProcessExtractor::initialize(const QString &pluginPath)
{
    return d->initialize(pluginPath);
}

bool ProcessExtractorPrivate::initialize(const QString &pluginPath)
{
    fmInfo() << "TextIndexCreatorApp: Initializing with plugin path:" << pluginPath;

    // Load plugins
    int pluginCount = m_pluginLoader.loadPlugins(pluginPath);
    if (pluginCount == 0) {
        fmWarning() << "TextIndexCreatorApp: No plugins loaded";
        return false;
    }

    fmInfo() << "TextIndexCreatorApp: Initialization complete, loaded" << pluginCount << "plugins";
    return true;
}

TEXTINDEX_CREATOR_END_NAMESPACE
