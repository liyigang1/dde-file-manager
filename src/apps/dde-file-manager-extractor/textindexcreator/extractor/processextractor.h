// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PROCESSEXTRACTOR_H
#define PROCESSEXTRACTOR_H

#include "indexextractor.h"
#include "pluginloader.h"

#include <QScopedPointer>

TEXTINDEX_CREATOR_BEGIN_NAMESPACE

class ProcessExtractorPrivate;

class ProcessExtractor : public IndexExtractor
{
public:
    ProcessExtractor();
    ~ProcessExtractor() override;

    Q_DISABLE_COPY(ProcessExtractor)

    IndexExtractionResult extract(const QString &filePath, size_t maxBytes = 0) const override;
    bool initialize(const QString &pluginPath);

private:
    const QScopedPointer<ProcessExtractorPrivate> d;
};

TEXTINDEX_CREATOR_END_NAMESPACE

#endif   // PROCESSEXTRACTOR_H
