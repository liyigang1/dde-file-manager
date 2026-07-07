// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FILEVIEWMENUHELPER_H
#define FILEVIEWMENUHELPER_H

#include "dfmplugin_workspace_global.h"

#include <DMenu>

#include <QObject>
#include <QString>

namespace dfmplugin_workspace {

class FileMenu : public DTK_NAMESPACE::Widget::DMenu {
    Q_OBJECT
public:
    explicit FileMenu(QWidget *parent = nullptr) :
        DTK_NAMESPACE::Widget::DMenu(parent) {}
    ~FileMenu() override {}
protected:
    virtual void paintEvent(QPaintEvent *event) override {
        DTK_NAMESPACE::Widget::DMenu::paintEvent(event);
        const auto &widget = qobject_cast<QWidget *>(parent());
        if (widget)
            widget->setUpdatesEnabled(true);
    }
};

class FileView;
class FileViewMenuHelper : public QObject
{
    Q_OBJECT
public:
    explicit FileViewMenuHelper(FileView *view = nullptr);
    static bool disableMenu();
    void showEmptyAreaMenu(const bool disableViewUpdates);
    void showNormalMenu(const QModelIndex &index, const Qt::ItemFlags &indexFlags, const bool disableViewUpdates);

    void setMenuScene(const QString &scene);
    void setWaitCursor();
    void reloadCursor();

private:
    QString currentMenuScene() const;

    FileView *view { nullptr };
    DTK_WIDGET_NAMESPACE::DMenu *menuPtr { nullptr };
};

}

#endif   // FILEVIEWMENUHELPER_H
