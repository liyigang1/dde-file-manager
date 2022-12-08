/*
 * Copyright (C) 2021 Uniontech Software Technology Co., Ltd.
 *
 * Author:     liuzhangjian<liuzhangjian@uniontech.com>
 *
 * Maintainer: liuzhangjian<liuzhangjian@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef ANYTHINGSEARCH_H
#define ANYTHINGSEARCH_H

#include "searchmanager/searcher/abstractsearcher.h"

#include <QSharedPointer>
#include <QElapsedTimer>
#include <QMutex>

class ComDeepinAnythingInterface;

DPSEARCH_BEGIN_NAMESPACE

class AnythingSearcher : public AbstractSearcher
{
    Q_OBJECT
    friend class TaskCommander;
    friend class TaskCommanderPrivate;

private:
    explicit AnythingSearcher(const QUrl &url, const QString &keyword, bool isBindPath, QObject *parent = nullptr);
    virtual ~AnythingSearcher() override;

    static bool isSupported(const QUrl &url, bool &isBindPath);
    bool search() override;
    void stop() override;
    bool hasItem() const override;
    QList<QUrl> takeAll() override;
    void tryNotify();

private:
    ComDeepinAnythingInterface *anythingInterface = nullptr;
    QAtomicInt status = kReady;
    QList<QUrl> allResults;
    mutable QMutex mutex;
    bool isBindPath;
    QString originalPath;

    //计时
    QElapsedTimer notifyTimer;
    qint64 lastEmit = 0;
};

DPSEARCH_END_NAMESPACE

#endif   // ANYTHINGSEARCH_H
