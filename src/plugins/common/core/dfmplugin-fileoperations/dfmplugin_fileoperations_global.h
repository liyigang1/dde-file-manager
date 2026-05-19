// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DFMPLUGIN_FILEOPERATIONS_GLOBAL_H
#define DFMPLUGIN_FILEOPERATIONS_GLOBAL_H

#include <dfm-base/dfm_log_defines.h>

#define DPFILEOPERATIONS_NAMESPACE dfmplugin_fileoperations

#define DPFILEOPERATIONS_BEGIN_NAMESPACE namespace DPFILEOPERATIONS_NAMESPACE {
#define DPFILEOPERATIONS_END_NAMESPACE }
#define DPFILEOPERATIONS_USE_NAMESPACE using namespace DPFILEOPERATIONS_NAMESPACE;

DPFILEOPERATIONS_BEGIN_NAMESPACE
DFM_LOG_USE_CATEGORY(DPFILEOPERATIONS_NAMESPACE)
DPFILEOPERATIONS_END_NAMESPACE

inline constexpr char kFileOperations[] { "org.deepin.dde.file-manager.operations" };
inline constexpr char kBlockEverySync[] { "file.operation.blockeverysync" };
inline constexpr char kSettingGroup[] { "10_advance.02_0external_storage_device" };
inline constexpr char kFileBigSize[] { "file.operation.bigfilesize" };
inline constexpr char kBroadcastPaste[] { "file.operation.broadcastpastevent" };
inline constexpr char kExpandDiskSync[] { "file.operation.expanddisksync" };
inline constexpr char kCIFSUseCopyFileRange[] { "file.operation.cifsusecopyfilerange" };

#endif   // DFMPLUGIN_FILEOPERATIONS_GLOBAL_H
