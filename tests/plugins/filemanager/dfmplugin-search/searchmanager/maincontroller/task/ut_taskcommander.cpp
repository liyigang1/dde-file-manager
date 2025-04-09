// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "searchmanager/maincontroller/task/taskcommander.h"
#include "searchmanager/maincontroller/task/taskcommander_p.h"
#include "searchmanager/searcher/iterator/iteratorsearcher.h"

#include "stubext.h"

#include <gtest/gtest.h>

DPSEARCH_USE_NAMESPACE

class TestSearcher : public AbstractSearcher
{
public:
    explicit TestSearcher(const QUrl &url, const QString &key, QObject *parent = nullptr)
        : AbstractSearcher(url, key, parent) {}

    bool search() { return true; }
    void stop() {}
    bool hasItem() const { return true; }
    DFMSearchResultMap takeAll() override { return DFMSearchResultMap(); }
};

// TaskCommanderPrivate
TEST(TaskCommanderPrivateTest, ut_working)
{
    stub_ext::StubExt st;

    TaskCommander task("taskId", QUrl("file:///home"), "key");
    TestSearcher searcher(QUrl("file:///home"), "key");
}

TEST(TaskCommanderPrivateTest, ut_createFileNameSearcher_1)
{
    stub_ext::StubExt st;

    TaskCommander task("taskId", QUrl("file:///home"), "key");
}

TEST(TaskCommanderPrivateTest, ut_createFileNameSearcher_2)
{
    stub_ext::StubExt st;

    TaskCommander task("taskId", QUrl("file:///home"), "key");
}

TEST(TaskCommanderPrivateTest, ut_createFileNameSearcher_3)
{
    stub_ext::StubExt st;

    TaskCommander task("taskId", QUrl("file:///home"), "key");
}

TEST(TaskCommanderPrivateTest, ut_onUnearthed)
{
    stub_ext::StubExt st;

    TaskCommander task("taskId", QUrl("file:///home"), "key");
    TestSearcher searcher(QUrl("file:///home"), "key");
}

TEST(TaskCommanderPrivateTest, ut_onFinished_1)
{
    stub_ext::StubExt st;
    st.set_lamda(&TaskCommander::deleteLater, [] {});

    TaskCommander task("taskId", QUrl("file:///home"), "key");
    task.d->deleted = true;

}

TEST(TaskCommanderPrivateTest, ut_onFinished_2)
{
    stub_ext::StubExt st;

    TaskCommander task("taskId", QUrl("file:///home"), "key");
    task.d->deleted = false;
}

// TaskCommander
TEST(TaskCommanderTest, ut_taskID)
{
    stub_ext::StubExt st;

    TaskCommander task("taskId", QUrl("file:///home"), "key");
    auto id = task.taskID();

    EXPECT_EQ(id, "taskId");
}

TEST(TaskCommanderTest, ut_getResults)
{
    stub_ext::StubExt st;

    TaskCommander task("taskId", QUrl("file:///home"), "key");
    auto result = task.getResults();

    EXPECT_FALSE(result.isEmpty());
}

TEST(TaskCommanderTest, ut_start_1)
{
    stub_ext::StubExt st;

    TaskCommander task("taskId", QUrl("file:///home"), "key");

    EXPECT_FALSE(task.start());
}

TEST(TaskCommanderTest, ut_start_2)
{
    stub_ext::StubExt st;

    TaskCommander task("taskId", QUrl("file:///home"), "key");

    EXPECT_TRUE(task.start());
}

TEST(TaskCommanderTest, ut_start_3)
{
    stub_ext::StubExt st;
    TestSearcher searcher(QUrl("file:///home"), "key");
    TaskCommander task("taskId", QUrl("file:///home"), "key");


    EXPECT_TRUE(task.start());
}

TEST(TaskCommanderTest, ut_stop)
{
    stub_ext::StubExt st;

    TestSearcher searcher(QUrl("file:///home"), "key");
    TaskCommander task("taskId", QUrl("file:///home"), "key");

    task.stop();
}

TEST(TaskCommanderTest, ut_deleteSelf_1)
{
    stub_ext::StubExt st;

    TaskCommander *task = new TaskCommander("taskId", QUrl("file:///home"), "key");
}

TEST(TaskCommanderTest, ut_deleteSelf_2)
{
    stub_ext::StubExt st;

    TaskCommander task("taskId", QUrl("file:///home"), "key");
    EXPECT_TRUE(task.d->deleted);
}

TEST(TaskCommanderTest, ut_createSearcher)
{
    stub_ext::StubExt st;

    TestSearcher searcher(QUrl(), "");

    TaskCommander task("taskId", QUrl("file:///home"), "key");
}
