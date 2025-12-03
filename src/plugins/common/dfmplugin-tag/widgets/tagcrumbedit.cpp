// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "tagcrumbedit.h"

#include <cmath>
#include <QAbstractTextDocumentLayout>

DWIDGET_USE_NAMESPACE
using namespace dfmplugin_tag;

TagCrumbEdit::TagCrumbEdit(QWidget *parent)
    : DCrumbEdit(parent)
{
    auto doc = QTextEdit::document();
    doc->setDocumentMargin(doc->documentMargin() + 5);
    setViewportMargins(3, 3, 3, 3);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    edit = qobject_cast<QTextEdit *>(this);
    if (edit) {
        QAbstractTextDocumentLayout *layout = edit->document()->documentLayout();
        connect(layout, &QAbstractTextDocumentLayout::documentSizeChanged, this, &TagCrumbEdit::updateHeight);
    }
}

bool TagCrumbEdit::isEditing()
{
    return isEditByDoubleClick;
}

void TagCrumbEdit::mouseDoubleClickEvent(QMouseEvent *event)
{
    isEditByDoubleClick = true;
    DCrumbEdit::mouseDoubleClickEvent(event);
    isEditByDoubleClick = false;
}

void TagCrumbEdit::updateHeight()
{
    qreal docHeight = edit->document()->size().height();
    QMargins margins = edit->contentsMargins();
    int totalHeight = std::ceil(docHeight) + margins.top() + margins.bottom();
    setFixedHeight(totalHeight);
}
