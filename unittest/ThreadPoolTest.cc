
#include "util/ThreadPool.h"
#include "future/Future.h"

#include <chrono>
#include <vector>

#include "gtest/gtest.h"

using ananas::ThreadPool;

static const int kMaxThreads = 4;
static const auto kLongTaskDuration = std::chrono::milliseconds(200);

class ThreadPoolTest : public testing::Test {
 public:
  ThreadPoolTest() = default;

  void SetUp() override { pool_.SetNumOfThreads(kMaxThreads); }
  void TearDown() override { pool_.JoinAll(); }

  int ShortTask() { return 42; }
  int LongTask() {
    std::this_thread::sleep_for(kLongTaskDuration);
    return 42;
  }

  void VoidTask(int& a) { a++; }

  ThreadPool pool_;
};

TEST_F(ThreadPoolTest, empty_test) {
  // threads are lazy create
  EXPECT_EQ(pool_.WorkerThreads(), 0);
  EXPECT_EQ(pool_.Tasks(), 0);
}

TEST_F(ThreadPoolTest, exec_task_test) {
  auto future = pool_.Execute(&ThreadPoolTest::ShortTask, this);
  EXPECT_EQ(future.Wait(), 42);

  EXPECT_EQ(pool_.WorkerThreads(), kMaxThreads);
  EXPECT_EQ(pool_.Tasks(), 0);
}

TEST_F(ThreadPoolTest, exec_many_task_test) {
  using namespace std::chrono;

  // post kMaxThreads tasks
  auto start = steady_clock::now();
  std::vector<ananas::Future<int>> futures;
  for (int i = 0; i < kMaxThreads; i++) {
    auto fut = pool_.Execute(&ThreadPoolTest::LongTask, this);
    futures.emplace_back(std::move(fut));
  }
  for (auto& f : futures) {
    void(f.Wait());
  }

  EXPECT_EQ(pool_.Tasks(), 0);

  auto usedMs = duration_cast<milliseconds>(steady_clock::now() - start);
  EXPECT_LE(usedMs.count(), 2 * kLongTaskDuration.count());
}

TEST_F(ThreadPoolTest, exec_too_many_task_test) {
  using namespace std::chrono;

  // post too many task
  const int extra = 2;
  auto start = steady_clock::now();
  std::vector<ananas::Future<int>> futures;
  for (int i = 0; i < extra + kMaxThreads; i++) {
    auto fut = pool_.Execute(&ThreadPoolTest::LongTask, this);
    futures.emplace_back(std::move(fut));
  }
  // let the workers take task first
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  EXPECT_EQ(pool_.Tasks(), extra);

  for (auto& f : futures) {
    void(f.Wait());
  }

  EXPECT_EQ(pool_.Tasks(), 0);

  auto usedMs = duration_cast<milliseconds>(steady_clock::now() - start);
  EXPECT_GE(usedMs.count(), 2 * kLongTaskDuration.count());
  EXPECT_LE(usedMs.count(), 3 * kLongTaskDuration.count());
}

TEST_F(ThreadPoolTest, exec_void_task_test) {
  int c = 0;
  auto future = pool_.Execute(&ThreadPoolTest::VoidTask, this, std::ref(c));
  future.Wait();

  EXPECT_EQ(c, 1);
}

TEST_F(ThreadPoolTest, join_test) {
  auto fut = pool_.Execute(&ThreadPoolTest::LongTask, this);
  pool_.JoinAll();

  // can still get task result.
  EXPECT_EQ(fut.Wait(), 42);

  ASSERT_THROW(pool_.Execute(&ThreadPoolTest::LongTask, this),
               std::runtime_error);
}
