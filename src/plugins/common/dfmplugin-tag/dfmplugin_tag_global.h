/*
 * Copyright (C) 2022 Uniontech Software Technology Co., Ltd.
 *
 * Author:     liuyangming<liuyangming@uniontech.com>
 *
 * Maintainer: zhengyouge<zhengyouge@uniontech.com>
 *             yanghao<yanghao@uniontech.com>
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
#ifndef DFMPLUGIN_TAG_GLOBAL_H
#define DFMPLUGIN_TAG_GLOBAL_H

#define DPTAG_NAMESPACE dfmplugin_tag

#define DPTAG_BEGIN_NAMESPACE namespace DPTAG_NAMESPACE {
#define DPTAG_END_NAMESPACE }
#define DPTAG_USE_NAMESPACE using namespace DPTAG_NAMESPACE;

#include <QObject>

DPTAG_BEGIN_NAMESPACE

enum class TagActionType : uint32_t {
    kGetAllTags = 1,
    kGetTagsThroughFile,
    kGetSameTagsOfDiffFiles,
    kGetFilesThroughTag,
    kGetTagsColor,
    kAddTags,
    kMakeFilesTags,
    kMakeFilesTagThroughColor,
    kRemoveTagsOfFiles,
    kDeleteTags,
    kDeleteFiles,
    kChangeTagsColor,
    kChangeTagsNameWithFiles,
    kChangeFilesPaths
};

inline constexpr int kTagDiameter { 10 };

namespace TagActionId {
inline constexpr char kActTagColorListKey[] { "tag-color-list" };
inline constexpr char kActTagAddKey[] { "tag-add" };
inline constexpr char kOpenFileLocation[] { "open-file-location" };
}

DPTAG_END_NAMESPACE

#endif   // DFMPLUGIN_TAG_GLOBAL_H
