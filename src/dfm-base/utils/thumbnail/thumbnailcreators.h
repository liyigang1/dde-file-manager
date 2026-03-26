// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef THUMBNAILCREATORS_H
#define THUMBNAILCREATORS_H

#include <dfm-base/dfm_base_global.h>
#include <dfm-base/dfm_global_defines.h>
#include <dfm-base/interfaces/fileinfo.h>

namespace dfmbase {
namespace ThumbnailCreators {
QImage defaultThumbnailCreator(const FileInfoPointer &info, DFMGLOBAL_NAMESPACE::ThumbnailSize size, const std::atomic_bool *stoped = nullptr);
QImage videoThumbnailCreator(const FileInfoPointer &info, DFMGLOBAL_NAMESPACE::ThumbnailSize size, const std::atomic_bool *stoped = nullptr);
QImage videoThumbnailCreatorFfmpeg(const FileInfoPointer &info, DFMGLOBAL_NAMESPACE::ThumbnailSize size, const std::atomic_bool *stoped = nullptr);
QImage videoThumbnailCreatorLib(const FileInfoPointer &info, DFMGLOBAL_NAMESPACE::ThumbnailSize size, const std::atomic_bool *stoped = nullptr);
QImage textThumbnailCreator(const FileInfoPointer &info, DFMGLOBAL_NAMESPACE::ThumbnailSize size, const std::atomic_bool *stoped = nullptr);
QImage audioThumbnailCreator(const FileInfoPointer &info, DFMGLOBAL_NAMESPACE::ThumbnailSize size, const std::atomic_bool *stoped = nullptr);
QImage imageThumbnailCreator(const FileInfoPointer &info, DFMGLOBAL_NAMESPACE::ThumbnailSize size, const std::atomic_bool *stoped = nullptr);
QImage djvuThumbnailCreator(const FileInfoPointer &info, DFMGLOBAL_NAMESPACE::ThumbnailSize size, const std::atomic_bool *stoped = nullptr);
QImage pdfThumbnailCreator(const FileInfoPointer &info, DFMGLOBAL_NAMESPACE::ThumbnailSize size, const std::atomic_bool *stoped = nullptr);
QImage appimageThumbnailCreator(const FileInfoPointer &info, DFMGLOBAL_NAMESPACE::ThumbnailSize size, const std::atomic_bool *stoped = nullptr);
}   // namespace ThumbnailCreators
}   // namespace dfmbase

#endif   // THUMBNAILCREATORS_H
