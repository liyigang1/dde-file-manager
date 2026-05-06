// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CONTENTDOCUMENTBUILDER_H
#define CONTENTDOCUMENTBUILDER_H

#include "indexdocumentbuilder.h"

TEXTINDEX_CREATOR_BEGIN_NAMESPACE

class ContentDocumentBuilder : public IndexDocumentBuilder
{
public:
    Lucene::DocumentPtr build(const QString &filePath, const QString &text) const override;
};

TEXTINDEX_CREATOR_END_NAMESPACE

#endif   // CONTENTDOCUMENTBUILDER_H
