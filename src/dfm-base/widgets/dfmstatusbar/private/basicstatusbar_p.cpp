// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "basicstatusbar_p.h"
#include "widgets/dfmstatusbar/basicstatusbar.h"

#include <DHorizontalLine>
#include <DAnchors>

#include <QLabel>
#include <QHBoxLayout>

using namespace dfmbase;

BasicStatusBarPrivate::BasicStatusBarPrivate(BasicStatusBar *qq)
    : QObject(qq),
      q(qq)
{
    initFormatStrings();
}

void BasicStatusBarPrivate::initFormatStrings()
{
    onlyOneItemCounted = tr("%1 item");
    counted = tr("%1 items");
    onlyOneItemSelected = tr("%1 item selected");
    selected = tr("%1 items selected");
    selectOnlyOneFolder = tr("%1 folder selected (contains %2)");
    selectFolders = tr("%1 folders selected (contains %2)");
    selectOnlyOneFile = tr("%1 file selected (%2)");
    selectFiles = tr("%1 files selected (%2)");
    selectedNetworkOnlyOneFolder = tr("%1 folder selected");
}

void BasicStatusBarPrivate::initTipLabel()
{
    tip = new DTK_WIDGET_NAMESPACE::DTipLabel(counted.arg("0"), q);
    tip->setMinimumWidth(30);
    tip->setContentsMargins(0, 0, 0, 0);
    tip->setAlignment(Qt::AlignCenter);
    tip->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
    tip->show();
}

void BasicStatusBarPrivate::initLayout()
{
    q->setFixedHeight(30);
    q->setContentsMargins(0, 0, 0, 0);
    auto vLayout = new QVBoxLayout(q);
    vLayout->setMargin(0);
    vLayout->setSpacing(0);
    auto line = new DTK_WIDGET_NAMESPACE::DHorizontalLine(q);
    line->setContentsMargins(0, 0, 0, 0);
    line->setLineWidth(1);
    vLayout->addWidget(line);

    layout = new QHBoxLayout;
    vLayout->addLayout(layout);

    q->clearLayoutAndAnchors();
    layout->addWidget(tip);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 4, 0);
}

void BasicStatusBarPrivate::calcFolderContains(const QList<QUrl> &folderList)
{
    discardCurrentJob();

    fileScanner.reset(new FileScanner());
    fileScanner->setOptions(FileScanner::ScanOption::SingleDepth | FileScanner::ScanOption::CountOnly);

    if (isJobDisconnect) {
        isJobDisconnect = false;
        initJobConnection();
    }

    fileScanner->start(folderList);
}

void BasicStatusBarPrivate::initJobConnection()
{
    if (!fileScanner)
        return;

    auto updateContains = [this](const FileScanner::ScanResult &result) {
        const int contains = result.fileCount + result.directoryCount;
        if (contains != folderContains) {
            folderContains = contains;
            q->updateStatusMessage();
        }
    };

    auto currentScanner = fileScanner;
    connect(currentScanner.data(), &FileScanner::progressChanged, this, updateContains);
    connect(currentScanner.data(), &FileScanner::finished, this, [currentScanner, updateContains](const FileScanner::ScanResult &result) {
        updateContains(result);
    });
}

void BasicStatusBarPrivate::discardCurrentJob()
{
    if (!fileScanner)
        return;

    fileScanner->disconnect();
    isJobDisconnect = true;

    if (fileScanner->isRunning()) {
        auto waitDeletePointer = fileScanner;
        connect(waitDeletePointer.data(), &FileScanner::finished, this, [this, waitDeletePointer] {
            waitDeleteScannerList.removeOne(waitDeletePointer);
        });
        fileScanner->stop();
        waitDeleteScannerList.append(fileScanner);
    }

    fileScanner = nullptr;
}
