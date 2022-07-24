#include "net/EventLoop.h"

#include <chrono>
#include <future>
#include <unordered_set>

#include "net/Application.h"
#include "net/Connection.h"
#include "net/DatagramSocket.h"
#include "gtest/gtest.h"

using namespace std::chrono;
using namespace ananas;

class EventLoopTest : public testing::Test {
 public:
  EventLoopTest() = default;
  ~EventLoopTest() = default;

  static void SetUpTestCase() {
    app_ = &Application::Instance();
    loop_ = app_->BaseLoop();
    thd_ = std::thread([]() { EventLoopTest::app_->Run(0, nullptr); });

    printf("SetUpTestCase\n");
  }

  static void TearDownTestCase() {
    printf("TearDownTestCase\n");
    app_->Exit();

    // must wait thd_ exit.
    thd_.join();
    app_->Reset();
  }

  static Application* app_;
  static EventLoop* loop_;
  static std::thread thd_;
  static int port_;
};

Application* EventLoopTest::app_{nullptr};
EventLoop* EventLoopTest::loop_{nullptr};
std::thread EventLoopTest::thd_;
int EventLoopTest::port_{9981};

TEST_F(EventLoopTest, timer_normal) {
  int count = 0;
  const int kMaxCount = 10;
  int timeout = 10;

  auto id = EventLoopTest::loop_->ScheduleAfterWithRepeat<kForever>(milliseconds(timeout), [&count, kMaxCount]() {
    if (count < kMaxCount) {
      ++count;
    }
  });

  // let timer run
  std::this_thread::sleep_for(milliseconds((kMaxCount * 2) * timeout));
  EXPECT_TRUE(EventLoopTest::loop_->Cancel(id));

  // check timer run
  EXPECT_EQ(count, kMaxCount);
}

TEST_F(EventLoopTest, cancel_before_run) {
  int count = 0;
  int timeout = 100;
  TimerId id = EventLoopTest::loop_->ScheduleAfterWithRepeat<kForever>(milliseconds(timeout), [&count]() {
    ++count;
    ASSERT_TRUE(!!!"Can't reach here!");
  });

  // cancel it at once
  EXPECT_TRUE(EventLoopTest::loop_->Cancel(id));

  // check cancel success
  std::this_thread::sleep_for(milliseconds(timeout));
  ASSERT_EQ(count, 0);
}

TEST_F(EventLoopTest, timer_cancel_during_run) {
  std::promise<void> ready, done;
  auto futReady = ready.get_future();
  auto futDone = done.get_future();

  int timeout = 10;
  TimerId id = EventLoopTest::loop_->ScheduleAfterWithRepeat<kForever>(milliseconds(timeout), [&]() {
    ready.set_value();

    std::this_thread::sleep_for(milliseconds(5));
    done.set_value();
  });

  // wait timer running
  (void)futReady.get();
  EXPECT_TRUE(EventLoopTest::loop_->Cancel(id));

  // already canceled
  EXPECT_FALSE(EventLoopTest::loop_->Cancel(id));

  // wait timer exit
  (void)futDone.get();
}

TEST_F(EventLoopTest, timer_cancel_self) {
  std::promise<void> cancel;
  auto futCancel = cancel.get_future();

  int timeout = 10;
  TimerId id;
  id = EventLoopTest::loop_->ScheduleAfterWithRepeat<kForever>(milliseconds(timeout), [&cancel, &id]() mutable {
    EXPECT_TRUE(EventLoopTest::loop_->Cancel(id));
    cancel.set_value();
  });

  // wait timer cancel
  (void)futCancel;

  // check really canceled
  EXPECT_FALSE(EventLoopTest::loop_->Cancel(id));
}

TEST_F(EventLoopTest, timer1_normal) {
  int count = 0;
  int timeout = 5;

  auto id = EventLoopTest::loop_->ScheduleAfter(milliseconds(timeout), [&count]() {
    ++count;
    EXPECT_TRUE(count == 1);
  });

  // let timer run
  std::this_thread::sleep_for(milliseconds(4 * timeout));
  // one shot timer is cancel automatically
  EXPECT_FALSE(EventLoopTest::loop_->Cancel(id));

  // check timer run
  EXPECT_EQ(count, 1);
}

TEST_F(EventLoopTest, timer1_cancel_before_run) {
  int count = 0;
  int timeout = 100;
  TimerId id = EventLoopTest::loop_->ScheduleAfter(milliseconds(timeout), [&count]() {
    ++count;
    ASSERT_TRUE(!!!"Can't reach here!");
  });

  // cancel it at once
  EXPECT_TRUE(EventLoopTest::loop_->Cancel(id));

  // check cancel success
  std::this_thread::sleep_for(milliseconds(timeout));
  ASSERT_EQ(count, 0);
}

TEST_F(EventLoopTest, timer1_cancel_self) {
  std::promise<void> cancel;
  auto futCancel = cancel.get_future();

  int timeout = 10;
  TimerId id;
  id = EventLoopTest::loop_->ScheduleAfter(milliseconds(timeout), [&cancel, &id]() mutable {
    EXPECT_TRUE(EventLoopTest::loop_->Cancel(id));
    cancel.set_value();
  });

  // wait timer cancel
  (void)futCancel.get();

  // check really canceled
  EXPECT_FALSE(EventLoopTest::loop_->Cancel(id));
}

TEST_F(EventLoopTest, tcp_stuff) {
  std::promise<void> done;
  std::unordered_set<int> conn_fds;
  app_->Listen("127.0.0.1", port_, [&done, &conn_fds](Connection* obj) {
    conn_fds.insert(obj->Identifier());
    obj->SetOnDisconnect([&done, &conn_fds](Connection* obj) {
      EXPECT_EQ(conn_fds.count(obj->Identifier()), 1);
      conn_fds.erase(obj->Identifier());
      done.set_value();
    });
  });

  std::weak_ptr<Connection> conn;
  std::promise<void> connected;
  app_->Connect("127.0.0.1", port_, [&conn, &connected](Connection* obj) {
    conn = std::static_pointer_cast<Connection>(obj->shared_from_this());
    connected.set_value();
  }, TcpConnFailCallback());

  connected.get_future().get();
  {
    auto sconn = conn.lock();
    EXPECT_NE(sconn.get(), nullptr);
    //EXPECT_TRUE(sconn->Connected());

    sconn->ActiveClose();

    //EXPECT_FALSE(sconn->Connected());
  }

  {
    // already closed
    auto sconn = conn.lock();
    EXPECT_EQ(sconn.get(), nullptr);

    // wait server got EOF
    done.get_future().get();
    EXPECT_TRUE(conn_fds.empty());
  }
}

TEST_F(EventLoopTest, tcp_connect_fail) {
  int wrong_port = port_ - 5;
  std::promise<void> failed;
  app_->Connect(
      "127.0.0.1", wrong_port, [](Connection* obj) { assert(0); },
      [&](EventLoop*, const SocketAddr& peer) {
        EXPECT_EQ(peer.GetPort(), wrong_port);
        failed.set_value();
      });

  failed.get_future().get();  // now on-fail has been called.
}

static void EchoUdp(DatagramSocket* obj, const char* data, int len) {
  EXPECT_LE(len, obj->GetMaxPacketSize());
  obj->SendPacket(data, len);
}

TEST_F(EventLoopTest, udp_stuff) {
  app_->ListenUDP("127.0.0.1", port_, EchoUdp, UDPCreateCallback());

  std::weak_ptr<DatagramSocket> conn;
  std::promise<void> create;
  app_->CreateClientUDP(EchoUdp, [&conn, &create](DatagramSocket* obj) {
    conn = std::static_pointer_cast<DatagramSocket>(obj->shared_from_this());

    EXPECT_FALSE(obj->SendPacket("hello", 5));  // no destination, failed

    SocketAddr dst("127.0.0.1", port_);
    EXPECT_TRUE(obj->SendPacket("hello", 5, &dst));
    create.set_value();
  });

  create.get_future().get();
  {
    auto sconn = conn.lock();
    EXPECT_NE(sconn.get(), nullptr);
  }
}

TEST_F(EventLoopTest, udp_big_packet) {
  std::promise<void> create;
  app_->CreateClientUDP(EchoUdp, [&create](DatagramSocket* obj) {
    std::string big_packet(4096, 'a');
    SocketAddr dst("127.0.0.1", port_);
    obj->SendPacket(big_packet.data(), big_packet.size(), &dst);

    create.set_value();
  });

  create.get_future().get();
}
