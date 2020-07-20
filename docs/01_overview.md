# 如何使用ananas

## 环境需求

  确保安装了CMake，且编译器支持C++11。

  另外操作系统必须是linux或MacOS。

## 编译与安装

  直接在源码根目录执行make;

  然后make install,默认安装在/usr/local目录下.

  如果想自定义安装路径，则给cmake添加参数:

  `cmake -DCMAKE_INSTALL_PREFIX=< your_install_path >`


  使用时，包含头文件和链接库类似如下语句：
  ```cpp
  // 包含头文件
  #include <ananas/net/Application.h>

  // 链接库
  g++ -std=c++11 *.cc -lananas_net -lananas_util -lpthread
  ```

## 程序语言

  目前只有C++11的实现，使用了大量C++11特性。

  计划不久推出ananas golang版。

## ananas的定位

ananas主要是出于教学目的而诞生，因此它作为rpc框架，非常小巧，只包含必不可少的组成。但不代表功能的缺失，麻雀虽小，五脏俱全。

ananas编译出来有3个静态库。分别是`libananas_util.a, libananas_net.a, libananas_rpc.a.`

其中net依赖util，rpc依赖net。

net和util是必须编译的，rpc在缺失google protobuf的情况下不会编译。

由于良好的代码组织，网络库与rpc严格分层，低耦合高内聚。你也可以只使用网络库编写代码，不需要rpc。这样ananas就退化成muduo这样的网络库了。

ananas功能十分健壮，网络库可以放心使用在生产环境。但rpc由于是基于future的，对开发人员要求较高，如果C++11较为精通，且对多线程、异步较为了解，大可放心使用。只是目前有一点残缺：由于精力有限也不想在周边花费时间，我没有提供较为正式的名字服务，只是简单的用redis作为服务发现。有兴趣的朋友可以自行修改添加。


## Hello ananas world

现在以一个简单的网络库echo应用程序结束这篇入门介绍教程。

首先说明下ananas应用都是从一个单例Application对象开始。

* **服务端编写**
 
服务端需要做的工作，首先是要监听的一个端口上，并能初始化accept的新连接。先看一下这一步怎么做:
  ```cpp
  // 获取单例Application对象
  auto& app = ananas::Application::Instance();

  // 监听在本地ip的6380端口，当accept到新连接，使用OnNewConnection函数初始化该连接。
  app.Listen("localhost", 6380, OnNewConnection);

  // 启动事件循环，不会结束，除非主动关闭(可以发送SIGINT信号优雅关闭)
  app.Run(argc, argv);
  ```

是不是很简单？不，还没有介绍OnNewConnection，它是关键所在。

  ```cpp
  // OnNewConnection必须符合`void (ananas::Connection*)`的签名，具体定义是`net/Typedefs.h`的`NewTcpConnCallback`。
  void OnNewConnection(ananas::Connection* conn) {
      using ananas::Connection;

      conn->SetOnMessage(OnMessage);
      conn->SetOnDisconnect([](Connection* conn)
                            {  
                                WRN(logger) << "OnDisConnect " << conn->Identifier();
                            });
  }
  ```
  OnNewConnection做了两件事：首先对新连接设置了OnMessage回调，然后设置了链接断开的处理：简单的打个日志而已。

继续，看看OnMessage怎么编写的：
  ```cpp
  size_t OnMessage(ananas::Connection* conn, const char* data, size_t len) {
      conn->SendPacket(data, len);  // echo package
      return len;       // the bytes consumed.
  }
  ```
  当一条连接上有数据到达时，亦即可读事件触发，那么ananas就会调用你的OnMessage函数处理数据。
  它的签名定义在`net/Typedefs.h`的`TcpMessageCallback`。
  
  重点解释一下参数的含义：
  首先是conn参数，表示当前数据来自哪条连接，Connection对象是对一个已连接tcp套接字的封装。
  data和len参数表示收到的数据以及长度；
  返回值是本次OnMessage消费的数据长度，非常重要，如果写错了返回值，程序将无法正常工作。

  echo服务比较简单，每次全消费完即可。由于tcp流式传输数据，对于其他协议来说，一个完整的消息可能
  需要多次传输才能得到，在消息接收完整之前，OnMessage只能返回0，让ananas帮忙缓存消息，直到完整接收。


  **一个完整的echo服务端就编写好了。**

* **客户端编写**

客户端需要做的工作，首先是connect服务器。先看一下这一步怎么做:
  
  ```cpp
  // 获取单例Application对象
  auto& app = ananas::Application::Instance();

  // 尝试连接本地ip的6380端口，当连接成功，调用OnNewConnection初始化连接
  // 当连接失败，调用OnConnFail
  // 设置了3秒钟连接超时，如果3秒内无法连接，也会调用OnConnFail
  app.Connect("localhost", port, OnNewConnection, OnConnFail, ananas::DurationMs(3000));

  // 启动事件循环，不会结束，除非主动关闭(可以发送SIGINT信号优雅关闭)
  app.Run(argc, argv);
  ```

好吧，客户端连接看上去比服务端监听复杂一点。不怕，下面一一解析。

  OnNewConnection就不再赘述，和服务端基本一致。下面看看OnConnFail的实现。

  ```cpp
  void OnConnFail(ananas::EventLoop* loop, const ananas::SocketAddr& peer) {  
      INF(logger) << "OnConnFail " << peer.ToString();
            
      // reconnect
      loop->ScheduleAfter(std::chrono::seconds(2),
                         [=]()
                         {  
                             loop->Connect(peer,
                                           OnNewConnection,
                                           OnConnFail,
                                           ananas::DurationMs(3000));
                         });
  }
  ```

  似乎信息量有点大，慢慢解释吧。
  首先第一个参数EventLoop，可以简单认为是个跑着epoll循环的线程。我们的connect处理是在这个loop发生的。
  peer参数是服务端地址。

  当连接失败，先打印一条日志，然后在loop中使用ScheduleAfter函数调度一个定时任务：两秒钟后尝试重连。
  事实上我们还可以添加重连次数，这里简单起见，无限次重连。

  **一个完整的ananas echo例子就结束了。**

  echo完整代码详见源码`tests/test_net`目录。

* 入门教程暂时结束了，更进一步的教程抽时间再写

