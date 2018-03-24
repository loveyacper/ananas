# future

    ananas提供了非常强大的promise&future实现，对异步编程、并发编程提供了良好的支持。
    尽管C++11也提供了promise，很遗憾，不支持then，只能以阻塞的方式使用，这限制了它的用途。
    ananas Promise在std::promise之上做了扩展，支持链式回调、超时回调、对多个并发操作的处理。

## future是ananas的核心

    ananas rpc框架的完全基于future和protobuf实现,RPC调用就是采用了基于future异步模式,因此如果要使用rpc,最好对future
    有一个深刻理解.

* **异步场景1:对一连串异步操作的处理**
  
  假如你需要从redis获取age的值，根据age是否大于18，再去获取对应的影片，然后播放。当然，这两步操作都是异步的。使用ananas的future，你可以这样做：
  ```cpp
  // Get是一个异步操作，返回一个future。
  // 先获取age的值，根据age是否大于18，决定获取适合他观看的影片.
  redisConn->Get("age")
            .Then([redisConn](const std::string& age) {
                // age变量就是"age"键对应的值
                if (std::stoi(age) >= 18)
                    // 你是成年人，现在**异步**获取成年影片
                    return redisConn->Get("AdultFilm");
                else
                    // 少儿不宜，看动画片吧
                    return redisConn->Get("ChildrenFilm");
            })
            .Then(
                // 获取影片的异步操作返回了,开始播放数据
                PlayFilm // PlayFile函数类型是  void (const std::string& filmContent);
            );
  ```
  利用future和C++11的lambda模拟closure，可以看到，在这一连串的异步操作中，我们甚至不需要保存上下文对象！

* **异步场景2:同时对多个服务发起请求，收集完所有响应后，开始处理。**
  
  玩家登陆游戏，后端服务需要向防沉迷系统查询该玩家的实名认证信息，又要向VIP服务器查询该玩家的VIP信息，同时收到响应后再做处理，比如VIP等级过高可能就无视防沉迷，我瞎编的逻辑。。:
  ```cpp
  // 同时发起两个异步请求，一个获取防沉迷信息，一个获取VIP信息
  auto fut1 = conn1->GetAntiAddictionInfo(player);
  auto fut2 = conn2->GetVIPInfo(player);
  ananas::WhenAll(fut1, fut2) // whenAll返回一个总的future，在其上注册回调
            .Then([clientConn](const std::tuple<AntiAddictInfo, VIPInfo>& results) {
                    // 两个请求都得到了响应
                    const auto& antiAddict = std::get<0>(results);
                    const auto& vipInfo = std::get<1>(results);
                    if (antiAddict.isFine || vipInfo.level > 15)
                        clientConn->SendPacket("登陆成功");
                    else
                        clientConn->SendPacket("登陆失败");
            });
  ```

* **异步场景3:同时对多个服务发起请求，只要收到一个响应，就开始处理。**
  
  这是whenAny的用武之地。这种场景有一个限制，就是请求的返回结果必须是同构的类型。
 
  假如同时向某服务的两台主机发起请求，收到任意一个响应就可以处理，默默丢弃后面的响应：
  ```cpp
  // 同时在两条连接上分别发起一样的异步请求
  auto fut1 = conn1->GetProfile(id);
  auto fut2 = conn2->GetProfile(id);
  ananas::WhenAny(fut1, fut2)
          .Then([conn1, conn2](const std::pair<size_t/* fut index*/, std::string>& result) {
                  size_t index = result.first;
                  std::cout << "服务器" << index << "返回了结果:"
                            << result.second << std::endl;;
              });
  ```

* **异步场景4:同时对多个服务发起请求，收到其中过半的响应，就开始处理**。
  
  这是whenN的用武之地。
  这个场景大家应该很熟悉，那就是分布式共识算法。不再举例，代码与上面类似，使用WhenN函数。

* **关于超时**
  
  ananas Future还提供了强大的超时回调，也就是说，它能保证在一定时间内，要么你的异步请求得到了响应，要么触发超时逻辑。
  需要ananas事件循环的支持。当然，如果你实现了Schedule接口，你也可以把ananas future的三个文件抽取出去单独使用。
  用法如下：

  ```cpp
  // 异步获取age，2秒内没返回就超时
  redisConn->Get("age")
            .Then([redisConn](const std::string& age) {
                std::cout << "age is " << age << std::endl;
            })
            .OnTimeout(std::chrono::seconds(2),/*两秒钟*/ []() {
                std::cout << "get age timeout " << std::endl;
            },
            &this_eventloop); //最后一个参数是Schedule接口实现，内置定时器支持。
  ```

* **关于同步操作的异步化**
  
  如果需要使用一些阻塞的调用，比如gethostbyname，你可能考虑到std::async。但是它返回了std::future，你需要轮询或阻塞等待才能得到结果。还有更糟的，如果永远阻塞呢？你无法处理超时。
  来吧，试试ananas Future结合线程池的加强版std::async：
  ```cpp
  ananas::ThreadPool pool;
  // 在线程池内执行系统调用，获取163.com的ip地址
  pool.Execute(::gethostbyname, "163.com")
      .Then([](const struct hostent* entry)) {
          // 成功获取163.com的ip地址
          std::cout << "163.com addr is " << entry->h_addr;
      })
      .OnTimeout(std::chrono::seconds(5), []() {
          // 5秒钟超时了
          std::cout << "gethost 163.com timeout! " << std::endl;
      },
      &eventloop);
  ```

* 关于future更详尽的介绍，可以看[这篇文章](https://loveyacper.github.io/ananas-future.html)

