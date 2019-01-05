# ananas protobuf rpc

<!-- vim-markdown-toc GFM -->
* [初步认识](#初步认识)
* [入门例子](#入门例子)
    * [编写proto](#编写proto)
    * [服务端](#服务端)
    * [客户端](#客户端)
    * [bootstrap](#bootstrap)
* [ananas协议设计](#ananas协议设计)

<!-- vim-markdown-toc -->

## 初步认识

ananas的rpc调用采用了future模式，调用方式如下：

```cpp
    auto timeout = std::chrono::seconds(3);
    rpc::Call<ResponseType>("ServiceName",  // 服务名字
                            "FunctionName", // 方法名字
                             request)       // 请求参数
                            .Then(ResponseHandler) // 响应处理函数
                            .OnTimeout(timeout, TimeoutHandler);
```

如上所示，发起rpc调用，需要指定响应类型，服务名，方法名，用户请求；
处理rpc响应，只需要使用Then注册处理函数；
处理rpc超时，只需要使用OnTimeout注册超时时间以及处理函数；
总体来说，使用比较简单，加上future的强大功能，这里很容易实现复杂的异步逻辑：

```cpp
    // 先发起一个到Service1的rpc请求,得到响应后,
    // 再发起一个到Service2的rpc请求.
    rpc::Call<ResponseType1>("Service1", "Func1", request1)
      .Then(ResponseHandler1_and_call_Service2)
      .Then(ResponseHandler2);


    // 同时发起到Service1和Service2的rpc请求
    // 都得到响应后,开始调用ResponseHandler12处理.
    auto c1 = rpc::Call<ResponseType1>("Service1", "Func1", request1);
    auto c2 = rpc::Call<ResponseType2>("Service2", "Func2", request2);

    WhenAll(c1, c2).Then(ResponseHandler12);

    // 类似还有WhenN和WhenAny,不再列举.
```

## 入门例子
### 编写proto
实现一个服务，提供两个函数功能：AppendDots将收到的字符串添加省略号，ToUpper将字符串转为大写。
```cpp
    syntax = "proto3";

    package test;

    option cc_generic_services = true;

    // sample
    message EchoRequest {
            string text = 1;
    }
    message EchoResponse {
            string text = 1;
    }

    service TestService {
        rpc AppendDots(EchoRequest) returns(EchoResponse) {
        }   

        rpc ToUpper(EchoRequest) returns(EchoResponse) {
        }   
    }
```

### 服务端
只截取一下ToUpper的实现，另一个函数类似。
```cpp
    class TestServiceImpl : public ::test::TestService {
        virtual void AppendDots(::google::protobuf::RpcController* ,
                                const ::test::EchoRequest* request,
                                ::test::EchoResponse* response,
                                ::google::protobuf::Closure* done) override final
        {
            // 实现具体函数逻辑
            std::string* answer = response->mutable_text();
            *answer = request->text();
            answer->append("...................");
                                                          
            // pb rpc的要求
            done->Run();
        }
    };
```
### 客户端
ananas rpc只提供了基于future的异步调用，方式如下:
```cpp
    test::EchoRequest req; //定义请求 
    req.set_text("hello world");

    // 发起调用,3秒钟没响应则打印超时
    Call<test::EchoResponse>("test.TestService",  // service name
                             "ToUpper",                      // method name
                             req)                            // request args
                            .Then(OnResponse)                // response handler
                            .OnTimeout(std::chrono::seconds(3), // timeout handler
                                       []() {  
                                           printf("AppendDots超时了\n");
                                       }, 
                                       server.BaseLoop());

    // 其中OnResponse是注册的回调函数，定义如下：
    void OnResponse(ananas::Try<test::EchoResponse>&& response) {
        // Try类型表示一个值，也可能表示一个异常
        // 所以在访问Try时候，可能会throw,必须使用try-catch语句块:
        try {
            // 正常处理
            test::EchoResponse rsp = std::move(response);
            USR(logger) << "OnResponse text " << rsp.text();
        } catch(const std::exception& exp) {
            // 异常处理，可能是服务连接不上，服务名或函数名异常等等
            USR(logger) << "OnResponse exception " << exp.what();
        }
    }
```
### bootstrap
启动RPC服务的步骤如下：
```cpp
    // 定义服务及其端口
    auto testsrv = new ananas::rpc::Service(new TestServiceImpl);
    testsrv->SetEndpoint(ananas::rpc::EndpointFromString("tcp://127.0.0.1:7890"));

    // bootstrap server
    ananas::rpc::Server server;
    // 设置3个工作线程
    server.SetNumOfWorker(3);
    // 注册刚才定义的服务
    server.AddService(testsrv);

    // 设置名字服务地址，内置默认的是redis，服务向redis注册服务名和endpoint
    // 客户端在Call之前，会自动访问redis得到此服务的endpoint列表，再发起真正的rpc调用
    server.SetNameServer("tcp://127.0.0.1:6379"); // 假设启动本地redis作为名字服务
    // 服务启动，进入无限循环
    server.Start();
```
启动RPC客户端类似，只是把Service换成ServiceStub.

## ananas协议设计
首先向netty致敬，完美的协议处理链使得实现任何协议都十分优雅。

ananas rpc也模仿了这样的设计，但在具体的实现过程中仍然遇到了很多问题，个人也反复思考了常见
的协议，比如http1.1，redis，websocket，https。最终思路如下：
默认内置的rpc协议是二进制协议；以client发起rpc请求为例，业务层的参数信息被称为message，代码中对应为google::protobuf::Message的子类；

但只有参数信息是不能直接发送的，框架底层还设计了RpcMessage消息类，包含了请求id，服务名、方法名等信息。
请求message将被序列化到RpcMessage的数据字段；这一步称为message到frame的转换，但这样还是不能够通过
tcp发送到网络，如果是udp倒是可以直接发送了.


第二步，需要对RpcMessage序列化，并添加4字节长度前缀，这一步称为frame到bytes的转换。
现在，请求可以从网络发送了，而且tcp的rpc服务器将能够解析请求，并定位到具体的方法调用。
(注意，目前暂缺bytes到bytes的转换。比如，如果在发送之前想对数据加密或压缩呢？这个转换功能是非常容易添加的)

上面描述的是二进制协议的请求发送。如果是文本协议呢？
比如HTTP1.1协议。在这种情况下，就没有第二步frame到bytes的转换.

例如，我们想发送如下请求
```cpp
    GET / HTTP/1.1
    Host: www.qq.com
```
首先定义proto协议：
```cpp
    message HttpRequest {
        string method = 1;
        string uri = 2;
        string version = 3;
        string host = 4;
    }
```
编写message到frame的转换函数：
```cpp
    bool EncodeHttpToFrame(const google::protobuf::Message* msg, ananas::rpc::RpcMessage& frame) {
        HttpRequest* req = static_cast<HttpRequest*>(msg);

        // 使用frame的数据字段存储序列化后的http请求
        auto request = frame.mutable_request();
        std::string* result = request->mutable_serialized_request();
        result->reserve(64);
        
        // first line
        result->append(req->method());
        result->append(" " + req->uri());
        result->append(" " + req->version());
        result->append("\r\n");

        // header
        result->append(req->host());
        result->append("\r\n\r\n");

        return true;
    }
```
给HTTP客户端stub注册channel回调,设置协议编解码函数:
```cpp
    void OnCreateHttpChannel(ananas::rpc::ClientChannel* channel)  { 
        //  对于一条新的HTTP连接，设置编解码函数
        ananas::rpc::Encoder encoder(nullptr); 
        encoder.SetMessageToFrameEncoder(EncodeHttpToFrame);
        channel->SetEncoder(std::move(encoder));
        //注意，这里没有设置frame到bytes的转换函数，
        //因此框架执行完message到frame的转换后，将直接
        //发送frame中的数据字段

        // 出于示例，这里省略了对解码的讨论
    }
```

只需要在new客户端stub的时候设置一下channel回调即可:
```cpp
    //每当一条新的http连接建立，将对其调用OnCreateHttpChannel
    httpServiceStub_->SetOnCreateChannel(OnCreateHttpChannel);
```

现在您可以这样发起HTTP请求:
```cpp
    HttpRequest get;
    get.set_method("GET");
    get.set_uri("/");
    get.set_version("HTTP/1.1");
    get.set_host("www.qq.com");

    ananas::rpc::Call<HttpResponse>("some-http-service",
                                    "some-method",
                                    get)
                                   .Then(OnHttpResponse);

```

这样，一个HTTP RPC客户端就编写好了！(当然有些细节并未展示出来，具体参见代码)

> PS: Http rpc服务端实现可参考`HealthService.cc`文件，ananas中内置的http监控服务。

## 待继续...

