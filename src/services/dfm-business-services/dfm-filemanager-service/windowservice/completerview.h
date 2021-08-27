/*
 * Copyright (C) 2020 ~ 2021 Uniontech Software Technology Co., Ltd.
 *
 * Author:     yanghao<yanghao@uniontech.com>
 *
 * Maintainer: zhengyouge<zhengyouge@uniontech.com>
 *             yanghao<yanghao@uniontech.com>
 *             hujianzhong<hujianzhong@uniontech.com>
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
#ifndef DFMCOMPLETERVIEW_H
#define DFMCOMPLETERVIEW_H

#include "dfm_filemanager_service_global.h"

#include <QCompleter>
#include <QListView>
#include <QStringListModel>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QFileSystemModel>

DSB_FM_BEGIN_NAMESPACE

class DFMCompleterViewDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit DFMCompleterViewDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

class DFMCompleterView :public QListView
{
    Q_OBJECT
public:
    explicit DFMCompleterView();

    QCompleter *completer();

    QStringListModel* model();

    DFMCompleterViewDelegate* itemDelegate();

Q_SIGNALS:
    void completerActivated(const QString &text);
    void completerActivated(const QModelIndex &index);
    void completerHighlighted(const QString &text);
    void completerHighlighted(const QModelIndex &index);

private:
    QCompleter m_completer;
    QStringListModel m_model;
    DFMCompleterViewDelegate m_delegate;
};

DSB_FM_END_NAMESPACE

#endif //DFMCOMPLETERVIEW_H
