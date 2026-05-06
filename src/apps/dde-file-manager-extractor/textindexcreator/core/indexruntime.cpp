// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "indexruntime.h"

TEXTINDEX_CREATOR_BEGIN_NAMESPACE

IndexRuntime::IndexRuntime(IndexProfile profile, QObject *parent)
    : QObject(parent),
      m_profile(std::move(profile)),
      m_context(m_profile, selectExtractor(), selectDocumentBuilder()),
      m_taskManager(new TaskManager(&m_context, this))
{
}

const IndexProfile &IndexRuntime::profile() const
{
    return m_profile;
}

const IndexContext &IndexRuntime::context() const
{
    return m_context;
}

TaskManager *IndexRuntime::taskManager() const
{
    return m_taskManager;
}

bool IndexRuntime::initialize(const QString &pluginPath)
{
    return m_processExtractor.initialize(pluginPath);
}

const IndexExtractor *IndexRuntime::selectExtractor() const
{
    return &m_processExtractor;
}

const IndexDocumentBuilder *IndexRuntime::selectDocumentBuilder() const
{
    switch (m_profile.type()) {
    case IndexProfile::Type::Content:
    default:
        return &m_contentDocumentBuilder;
    }
}

TEXTINDEX_CREATOR_END_NAMESPACE
