// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef THUMBNAILWORKER_H
#define THUMBNAILWORKER_H

#include <dfm-base/dfm_base_global.h>
#include <dfm-base/dfm_global_defines.h>
#include <dfm-base/interfaces/fileinfo.h>

#include <QUrl>

#include <functional>

namespace dfmbase {

class ThumbnailWorkerPrivate;
class ThumbnailWorker : public QObject
{
    Q_OBJECT
public:
    using ThumbnailTaskMap = QMap<QUrl, DFMGLOBAL_NAMESPACE::ThumbnailSize>;

    explicit ThumbnailWorker(QObject *parent = nullptr);
    ~ThumbnailWorker();

    using ThumbnailCreator = std::function<QImage(const FileInfoPointer &, DFMGLOBAL_NAMESPACE::ThumbnailSize, const std::atomic_bool *)>;
    bool registerCreator(const QString &mimeType, ThumbnailCreator creator);
    void stop();

public Q_SLOTS:
    void onTaskAdded(const ThumbnailTaskMap &taskMap);

Q_SIGNALS:
    void thumbnailCreateFinished(const QUrl &url, const QString &thumbnail);
    void thumbnailCreateFailed(const QUrl &url);

private:
    void createThumbnail(QUrl origUrl, const FileInfoPointer &info, Global::ThumbnailSize size, const QUrl &saveUrl = QUrl());

private:
    QScopedPointer<ThumbnailWorkerPrivate> d;
};
}   // namespace dfmbase

#endif   // THUMBNAILWORKER_H
