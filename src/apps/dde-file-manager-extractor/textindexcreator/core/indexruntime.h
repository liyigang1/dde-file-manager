// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef INDEXRUNTIME_H
#define INDEXRUNTIME_H

#include "core/indexcontext.h"
#include "document/contentdocumentbuilder.h"
#include "extractor/processextractor.h"
#include "profile/indexprofile.h"
#include "task/taskmanager.h"

#include <QObject>

TEXTINDEX_CREATOR_BEGIN_NAMESPACE

class IndexRuntime : public QObject
{
    Q_OBJECT

public:
    explicit IndexRuntime(IndexProfile profile, QObject *parent = nullptr);

    const IndexProfile &profile() const;
    const IndexContext &context() const;

    TaskManager *taskManager() const;

    bool initialize(const QString &pluginPath);

private:
    const IndexExtractor *selectExtractor() const;
    const IndexDocumentBuilder *selectDocumentBuilder() const;

    IndexProfile m_profile;
    ProcessExtractor m_processExtractor;
    ContentDocumentBuilder m_contentDocumentBuilder;
    IndexContext m_context;
    TaskManager *m_taskManager { nullptr };
};

TEXTINDEX_CREATOR_END_NAMESPACE

#endif   // INDEXRUNTIME_H
