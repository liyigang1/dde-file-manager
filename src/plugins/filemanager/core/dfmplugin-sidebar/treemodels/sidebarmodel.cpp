// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sidebarmodel.h"
#include "sidebaritemdelegate.h"
#include "sidebaritem.h"
#include "utils/sidebarhelper.h"
#include "utils/sidebarinfocachemananger.h"
#include "utils/sidebarfilewatcher.h"

#include <dfm-base/base/schemefactory.h>
#include <dfm-base/utils/universalutils.h>
#include <dfm-framework/event/event.h>

#include <QMimeData>
#include <QDebug>
#include <QtConcurrent>

DPSIDEBAR_USE_NAMESPACE
DFMBASE_USE_NAMESPACE

/*!
 * \class SideBarModel
 * \brief
 */
SideBarModel::SideBarModel(QObject *parent)
    : QStandardItemModel(parent)
{
    // 初始化文件监听器
    fileWatcher = new SidebarFileWatcher(this);
    connect(fileWatcher, &SidebarFileWatcher::directoryCreated, this, &SideBarModel::onDirectoryCreated);
    connect(fileWatcher, &SidebarFileWatcher::directoryRemoved, this, &SideBarModel::onDirectoryRemoved);
    connect(fileWatcher, &SidebarFileWatcher::directoryRenamed, this, &SideBarModel::onDirectoryRenamed);
}

bool SideBarModel::canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const
{
    // when drag onto the empty space of the area, just return false.
    if (column == -1 || row == -1 || !data)
        return false;

    Q_ASSERT(column == 0);

    auto isSeparator = [](SideBarItem *item) -> bool {
        return item && dynamic_cast<SideBarItemSeparator *>(item);
    };
    auto isItemDragEnabled = [](SideBarItem *item) -> bool {
        return item && item->flags().testFlag(Qt::ItemIsDragEnabled);
    };
    auto isTheSameGroup = [](SideBarItem *item1, SideBarItem *item2) -> bool {
        return item1 && item2 && item1->group() == item2->group();
    };

    SideBarItem *targetItem = this->itemFromIndex(row, parent);

    if (isSeparator(targetItem))   //According to the requirement，sparator does not support to drop.
        return false;

    // check if is item internal move by action and mimetype:
    if (action == Qt::MoveAction) {
        SideBarItem *sourceItem = curDragItem;

        // normal drag tag or bookmark or quick access
        if (isItemDragEnabled(targetItem) && isTheSameGroup(sourceItem, targetItem))
            return true;

        SideBarItem *prevItem = itemFromIndex(row - 1, parent);
        // drag tag item to bottom, targetItem is null
        // drag bookmark item on the bookmark bottom separator, targetItem is Separator
        if ((!targetItem || isSeparator(targetItem)) && sourceItem != prevItem)
            return isItemDragEnabled(prevItem) && isTheSameGroup(prevItem, sourceItem);

        return false;
    }

    return QStandardItemModel::canDropMimeData(data, action, row, column, parent);
}

bool SideBarModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    if (!canDropMimeData(data, action, row, column, parent))
        return false;

    return QStandardItemModel::dropMimeData(data, action, row, column, parent);
}

QMimeData *SideBarModel::mimeData(const QModelIndexList &indexes) const
{
    curDragItem = nullptr;
    QMimeData *data = QStandardItemModel::mimeData(indexes);
    if (!data)
        return nullptr;
    if (!indexes.isEmpty())
        curDragItem = itemFromIndex(indexes.first().row(), indexes.first().parent());
    return data;
}

SideBarItem *SideBarModel::itemFromIndex(const QModelIndex &index) const
{
    QStandardItem *item = QStandardItemModel::itemFromIndex(index);
    SideBarItem *castedItem = static_cast<SideBarItem *>(item);

    return castedItem;
}

SideBarItem *SideBarModel::itemFromIndex(int index, const QModelIndex &parent) const
{
    return itemFromIndex(this->index(index, 0, parent));
}

QList<SideBarItemSeparator *> SideBarModel::groupItems() const
{
    QList<SideBarItemSeparator *> items;

    for (int i = 0; i != rowCount(); ++i) {
        auto item = itemFromIndex(i);
        SideBarItemSeparator *groupItem = dynamic_cast<SideBarItemSeparator *>(item);
        if (groupItem)
            items.append(groupItem);
    }

    return items;
}

QList<SideBarItem *> SideBarModel::subItems() const
{
    QList<SideBarItem *> items;
    QList<SideBarItemSeparator *> groups { groupItems() };

    for (auto groupItem : groups) {
        Q_ASSERT(groupItem);
        int childCount = groupItem->rowCount();
        for (int i = 0; i != childCount; ++i) {
            QStandardItem *childItem = groupItem->child(i);
            SideBarItem *subItem = static_cast<SideBarItem *>(childItem);
            if (subItem)
                items.append(subItem);
        }
    }

    return items;
}

QList<SideBarItem *> SideBarModel::subItems(const QString &groupName) const
{
    QList<SideBarItem *> items;
    QList<SideBarItemSeparator *> groups { groupItems() };

    for (auto groupItem : groups) {
        Q_ASSERT(groupItem);
        if (groupItem->group() != groupName)
            continue;
        int childCount = groupItem->rowCount();
        for (int i = 0; i != childCount; i++) {
            QStandardItem *childItem = groupItem->child(i);
            SideBarItem *subItem = static_cast<SideBarItem *>(childItem);
            if (subItem)
                items.append(subItem);
        }
    }
    return items;
}

bool SideBarModel::insertRow(int row, SideBarItem *item)
{
    if (!item)
        return false;

    if (0 > row)
        return false;

    if (findRowByUrl(item->url()).row() > 0)
        return true;

    SideBarItemSeparator *groupItem = dynamic_cast<SideBarItemSeparator *>(item);
    if (groupItem) {   //top item
        QStandardItemModel::insertRow(row + 1, item);   //insert the top item
        return true;
    } else {   //sub item
        int count = this->rowCount();
        for (int i = 0; i < count; i++) {
            const QModelIndex &index = this->index(i, 0);
            if (!index.isValid())
                continue;
            if (index.data(SideBarItem::Roles::kItemGroupRole).toString() != item->group())
                continue;
            SideBarItem *groupItem = this->itemFromIndex(index);
            if (groupItem) {
                int rows = groupItem->rowCount();
                if (row == 0 || (row > 0 && row < rows))
                    groupItem->insertRow(row, item);
                else if (row >= rows)
                    groupItem->appendRow(item);
                else if (row == -1)
                    groupItem->insertRow(0, item);
            }
            return true;
        }
    }

    return true;
}

int SideBarModel::appendRow(SideBarItem *item, bool direct)
{
    if (!item)
        return -1;

    auto r = findRowByUrl(item->url()).row();
    if (r > 0)
        return r;

    SideBarItemSeparator *topItem = dynamic_cast<SideBarItemSeparator *>(item);
    SideBarItem *groupOther = nullptr;
    if (topItem) {   //Top item
        auto t = topItem->group();
        QStandardItemModel::appendRow(item);
        return rowCount() - 1;   //The return value is the index of top item.
    } else {   //Sub item
        int count = this->rowCount();
        for (int i = 0; i < count; i++) {
            const QModelIndex &index = this->index(i, 0);
            if (!index.isValid())
                continue;
            QString groupId = index.data(SideBarItem::Roles::kItemGroupRole).toString();
            if (groupId == DefaultGroup::kOther)
                groupOther = this->itemFromIndex(i);
            if (groupId != item->group())
                continue;
            SideBarItem *groupItem = this->itemFromIndex(i);
            bool itemInserted = false;
            int row = 0;
            for (; !direct && row < groupItem->rowCount(); row++) {
                QStandardItem *childItem = groupItem->child(row);
                auto tmpItem = dynamic_cast<SideBarItem *>(childItem);
                if (!tmpItem)
                    continue;

                //Sort for devices group and network group, all so for quick access group.
                //Both of Computer plugin and bookmark plugin are following the the `hook_Group_Sort` event.
                bool sorted = { dpfHookSequence->run("dfmplugin_sidebar", "hook_Group_Sort", groupId, item->subGourp(), item->url(), tmpItem->url()) };
                if (sorted) {
                    groupItem->insertRow(row, item);
                    itemInserted = true;
                    break;
                }
            }
            if (!itemInserted)
                groupItem->appendRow(item);

            return row;   // The position after sorted
        }
    }
    if (groupOther && !topItem) {   //If can not find out the parent item, just append it to Group_Other
        groupOther->appendRow(item);
        fmInfo() << "Item added to groupOther";
        return groupOther->rowCount() - 1;
    }
    QStandardItemModel::appendRow(item);
    fmInfo() << "Item added to the end of sidebar.";
    return rowCount() - 1;
}

bool SideBarModel::removeRow(const QUrl &url)
{
    if (!url.isValid())
        return false;

    int count = this->rowCount();
    for (int i = 0; i < count; i++) {
        const QModelIndex &index = this->index(i, 0);   //top item index
        if (index.isValid()) {
            QStandardItem *item = qobject_cast<const SideBarModel *>(index.model())->itemFromIndex(index);
            SideBarItemSeparator *groupItem = dynamic_cast<SideBarItemSeparator *>(item);
            if (!groupItem)
                continue;
            int childCount = groupItem->rowCount();
            for (int j = 0; j < childCount; j++) {
                QStandardItem *childItem = groupItem->child(j);
                SideBarItem *subItem = static_cast<SideBarItem *>(childItem);
                if (!subItem)
                    continue;
                if (DFMBASE_NAMESPACE::UniversalUtils::urlEquals(subItem->url(), url)) {
                    QStandardItemModel::removeRows(j, 1, groupItem->index());
                    return true;
                }
            }
        }
    }

    return false;
}

void SideBarModel::updateRow(const QUrl &url, const ItemInfo &newInfo)
{
    if (!url.isValid())
        return;
    for (int r = 0; r < rowCount(); r++) {
        auto item = itemFromIndex(r);   //Top item
        SideBarItemSeparator *groupItem = dynamic_cast<SideBarItemSeparator *>(item);
        if (!groupItem)
            continue;
        int childCount = groupItem->rowCount();
        for (int j = 0; j < childCount; j++) {
            QStandardItem *childItem = groupItem->child(j);
            SideBarItem *subItem = static_cast<SideBarItem *>(childItem);
            if (!subItem)
                continue;
            bool foundByCb = subItem->itemInfo().findMeCb && subItem->itemInfo().findMeCb(subItem->url(), url);

            if (foundByCb || DFMBASE_NAMESPACE::UniversalUtils::urlEquals(subItem->url(), url)) {
                subItem->setIcon(newInfo.icon);
                subItem->setText(newInfo.displayName);
                subItem->setUrl(newInfo.url);
                subItem->setFlags(newInfo.flags);
                subItem->setGroup(newInfo.group);
                Qt::ItemFlags flags = subItem->flags();
                if (newInfo.isEditable)
                    flags |= Qt::ItemIsEditable;
                else
                    flags &= (~Qt::ItemIsEditable);
                subItem->setFlags(flags);
                return;
            }
        }
    }
}

void dfmplugin_sidebar::SideBarModel::setCanRemoveRows(bool can)
{
    canRemoveRows = can;
}

bool SideBarModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (canRemoveRows)
        return QStandardItemModel::removeRows(row, count, parent);
    else
        return false;
}

QModelIndex SideBarModel::findRowByUrl(const QUrl &url) const
{
    // 递归查找 URL 匹配的项
    return findRowByUrlRecursive(url, QModelIndex());
}

QModelIndex SideBarModel::findRowByUrlRecursive(const QUrl &url, const QModelIndex &parent) const
{
    // 获取当前层级的行数
    int rowCount = this->rowCount(parent);

    for (int i = 0; i < rowCount; ++i) {
        QModelIndex index = this->index(i, 0, parent);
        if (!index.isValid())
            continue;

        // 获取当前项
        SideBarItem *item = itemFromIndex(index);
        if (!item)
            continue;

        // 检查 url 是否匹配
        if (DFMBASE_NAMESPACE::UniversalUtils::urlEquals(url, item->url()))
            return index;

        // 检查 finalUrl 是否匹配
        QUrl finalUrl = item->targetUrl();
        if (!finalUrl.isEmpty() && DFMBASE_NAMESPACE::UniversalUtils::urlEquals(url, finalUrl))
            return index;

        // 递归检查子项
        QModelIndex childIndex = findRowByUrlRecursive(url, index);
        if (childIndex.isValid())
            return childIndex;
    }

    return QModelIndex();
}

void SideBarModel::addEmptyItem()
{
    //Attention!
    //The current sidebar does not support external plugins to add groups.
    //If this feature is implemented in the future, it is necessary to move the emptyItem item appropriately
    int count = rowCount();
    QSize emptyItemsize = QSize(10, 10);
    if (count > 0) {
        QStandardItem *lastItem = item(count - 1);
        if (lastItem && lastItem->sizeHint() == emptyItemsize)
            return;
    }

    beginInsertRows(QModelIndex(), rowCount(), rowCount());

    auto emptyItem = new QStandardItem("");
    emptyItem->setFlags(Qt::NoItemFlags);
    emptyItem->setSizeHint(emptyItemsize);

    QStandardItemModel::appendRow(emptyItem);
    endInsertRows();
}

// 添加文件监听相关的方法
void SideBarModel::onItemExpanded(const QModelIndex &index)
{
    // 获取展开项的 URL
    SideBarItem *item = itemFromIndex(index);
    if (!item)
        return;

    // 使用 targetUrl() 获取设备的真实路径
    QUrl url = item->targetUrl();
    if (url.isEmpty())
        url = item->url();

    // 启动文件监听器
    if (fileWatcher) {
        fileWatcher->watchDirectory(url);
    }
}

void SideBarModel::onItemCollapsed(const QModelIndex &index)
{
    // 获取折叠项的 URL
    SideBarItem *item = itemFromIndex(index);
    if (!item)
        return;

    // 使用 targetUrl() 获取设备的真实路径
    QUrl url = item->targetUrl();
    if (url.isEmpty())
        url = item->url();

    // 停止文件监听器
    if (fileWatcher) {
        fileWatcher->unwatchDirectory(url);
    }
}

void SideBarModel::addSubItems(const QModelIndex &index, const QList<QUrl> &urls)
{
    SideBarItem *parentItem = itemFromIndex(index);
    if (!parentItem) {
        fmWarning() << "cannot find parent sidebar item!" << index;
        return;
    }

    for (auto url : urls)
        addSubItem(index, url);
}

void SideBarModel::onDirectoryCreated(const QUrl &parentUrl, const QUrl &url)
{
    // 在树视图中添加新目录
    QModelIndex parentIndex = findRowByUrl(parentUrl);
    if (!parentIndex.isValid()) {
        fmDebug() << "Parent directory not found in sidebar:" << parentUrl;
        return;
    }

    addSubItem(parentIndex, url);
}

void SideBarModel::onDirectoryRemoved(const QUrl &parentUrl, const QUrl &url)
{
    // 在树视图中移除目录
    QModelIndex index = findRowByUrl(url);
    if (!index.isValid()) {
        fmDebug() << "Directory not found in sidebar:" << url;
        return;
    }

    // 获取父项
    QModelIndex parentIndex = index.parent();
    if (!parentIndex.isValid()) {
        fmDebug() << "Parent index is invalid";
        return;
    }

    // 获取父项
    QStandardItem *parentItem = itemFromIndex(parentIndex);
    if (!parentItem) {
        fmDebug() << "Failed to get parent item from index";
        return;
    }

    // 获取要删除的项
    SideBarItem *itemToRemove = itemFromIndex(index);
    if (!itemToRemove) {
        fmDebug() << "Failed to get item from index";
        return;
    }

    // 检查父项是否为 separator，如果是，则说明当前项是分区，只需要移除当前项下的所有子项
    SideBarItemSeparator *separatorItem = dynamic_cast<SideBarItemSeparator *>(parentItem);
    if (separatorItem) {
        // 如果父项是 separator，则只移除当前项下的所有子项
        fmDebug() << "Parent item is a separator, removing all children of:" << url;

        // 获取当前项下的所有子项
        int childCount = itemToRemove->rowCount();
        if (childCount > 0) {

            // 通知视图即将移除所有子项
            beginRemoveRows(index, 0, childCount - 1);

            // 移除所有子项
            for (int i = childCount - 1; i >= 0; --i) {
                itemToRemove->removeRow(i);
            }

            // 通知视图移除完成
            endRemoveRows();

            fmDebug() << "Removed" << childCount << "children from:" << url;
        }

        // 在移除子项后，发出折叠该项的信号
        emit requestCollapseItem(index);
        fmDebug() << "Requested to collapse item after removed children:" << url;

    } else {
        // 如果父项不是 separator，则移除当前项本身
        fmDebug() << "Removing item:" << url;
        SideBarInfoCacheMananger::instance()->removeItemInfoCache(url);

        // 通知视图即将移除行
        // beginRemoveRows(parentIndex, index.row(), index.row());

        // 移除子项
        parentItem->removeRow(index.row());

        // 通知视图移除完成
        // endRemoveRows();

        fmDebug() << "Item removed from sidebar:" << url;
    }
}

void SideBarModel::onDirectoryRenamed(const QUrl &parentUrl, const QUrl &oldUrl, const QUrl &newUrl)
{
    // 先移除旧的项
    onDirectoryRemoved(parentUrl, oldUrl);

    // 再添加新的项
    onDirectoryCreated(parentUrl, newUrl);
}

void SideBarModel::addSubItem(const QModelIndex &index, const QUrl &url)
{
    // 获取父目录项
    SideBarItem *parentItem = itemFromIndex(index);
    if (!parentItem) {
        fmDebug() << "Failed to get parent item from index";
        return;
    }

    // 检查是否已存在相同的项
    int childCount = parentItem->rowCount();
    for (int i = 0; i < childCount; ++i) {
        SideBarItem *childItem = dynamic_cast<SideBarItem *>(parentItem->child(i));
        if (childItem) {
            // 检查 URL 是否相同
            if (DFMBASE_NAMESPACE::UniversalUtils::urlEquals(url, childItem->url())) {
                fmDebug() << "Directory already exists in sidebar:" << url;
                return;   // 如果已存在相同的项，直接返回
            }
        }
    }

    // 获取父项的组别
    QString group = parentItem->group();

    // 创建新的侧边栏项
    auto info = dfmbase::InfoFactory::create<dfmbase::FileInfo>(url);
    QString fileName = info ? info->fileName() : "Unknown";
    QIcon folderIcon = QIcon::fromTheme("folder");

    // 直接构造 SideBarItem 对象
    SideBarItem *item = new SideBarItem(folderIcon, fileName, group, url);
    ItemInfo itemInfo = ItemInfo(url, { { PropertyKey::kItemExpandable, true }, { PropertyKey::kFinalUrl, url }, { PropertyKey::kGroup, DefaultGroup::kDevice } });
    SideBarInfoCacheMananger::instance()->addItemInfoCache(itemInfo);

    if (!item) {
        fmDebug() << "Failed to create new sidebar item";
        return;
    }

    // 禁用编辑功能，不允许双击重命名
    Qt::ItemFlags flags = item->flags();
    flags &= ~Qt::ItemIsEditable;
    flags &= ~Qt::ItemIsDragEnabled;
    item->setFlags(flags);

    // 获取当前父目录中的所有子项
    QString newName = fileName;

    // 查找正确的插入位置（按字母顺序）
    int insertRow = childCount;   // 默认插入到末尾

    for (int i = 0; i < childCount; ++i) {
        SideBarItem *childItem = dynamic_cast<SideBarItem *>(parentItem->child(i));
        if (childItem) {
            QString childName = childItem->text();
            // 转换为小写进行比较，确保排序一致性
            if (newName.toLower() < childName.toLower()) {
                insertRow = i;
                break;
            }
        }
    }

    // 通知视图即将插入行
    // beginInsertRows(index, insertRow, insertRow);

    // 插入新项
    parentItem->insertRow(insertRow, item);

    // 通知视图插入完成
    // endInsertRows();

    // fmDebug() << "Directory added to sidebar:" << url;
}
