// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef INDEXCONTEXT_H
#define INDEXCONTEXT_H

#include "textindexcreator/textindexcreatorservice.h"
#include "profile/indexprofile.h"
#include "state/indexstatestore.h"

SERVICETEXTINDEX_BEGIN_NAMESPACE

class IndexContext
{
public:
    IndexContext(IndexProfile profile,
                 const IndexStateStore *stateStore,
                 TextIndexCreatorService *sevice)
        : m_profile(std::move(profile)),
          m_stateStore(stateStore),
          m_extractService(sevice)
    {
    }

    const IndexProfile &profile() const
    {
        return m_profile;
    }

    const IndexStateStore *stateStore() const
    {
        return m_stateStore;
    }

    TextIndexCreatorService *extractorService() const
    {
        return m_extractService;
    }

private:
    IndexProfile m_profile;
    const IndexStateStore *m_stateStore { nullptr };
    TextIndexCreatorService *m_extractService { nullptr };
};

SERVICETEXTINDEX_END_NAMESPACE

#endif   // INDEXCONTEXT_H
