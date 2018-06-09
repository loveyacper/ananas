# ananas protobuf rpc


<!-- vim-markdown-toc GFM -->
* [入门例子](#入门例子)
    * [编写proto](#编写proto)
    * [服务端](#服务端)
    * [客户端](#客户端)
    * [bootstrap](#bootstrap)
* [ananas协议设计](#ananas协议设计)
* [文档待补全](#文档待补全)
    
<!-- vim-markdown-toc -->

## 入门例子
### 编写proto
实现一个服务，提供两个函数功能：AppendDots将收到的字符串添加省略号，ToUpper将字符串转为大写。
```cpp
    syntax = "proto3";

    package ananas.rpc.test;

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
    class TestServiceImpl : public ::ananas::rpc::test::TestService {
        virtual void AppendDots(::google::protobuf::RpcController* ,
                                const ::ananas::rpc::test::EchoRequest* request,
                                ::ananas::rpc::test::EchoResponse* response,
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
    ananas::rpc::test::EchoRequest req; //定义请求 
    req.set_text("hello world");

    // 发起调用,3秒钟没响应则打印超时
    Call<test::EchoResponse>("ananas.rpc.test.TestService",  // service name
                             "ToUpper",                      // method name
                             req)                            // request args
                            .Then(OnResponse)                // response handler
                            .OnTimeout(std::chrono::seconds(3), // timeout handler
                                       []()
                                       {  
                                           printf("AppendDots超时了\n");
                                       }, 
                                       server.BaseLoop());

    // 其中OnResponse是注册的回调函数，定义如下：
    void OnResponse(ananas::Try<ananas::rpc::test::EchoResponse>&& response) {
        // Try类型表示一个值，也可能表示一个异常
        // 所以在访问Try时候，可能会throw,必须使用try-catch语句块:
        try {
            // 正常处理
            ananas::rpc::test::EchoResponse rsp = std::move(response);
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
首先向netty致敬,完美的协议处理链使得实现任何协议都十分优雅。

ananas rpc也模仿了这样的设计,但在具体的实现过程中仍然遇到了很多问题,个人也反复思考了常见
的协议，比如http1.1，redis，websocket，https。最终思路如下：
首先默认的rpc协议是二进制协议；以client发起rpc请求为例，业务层的参数信息被称为message，代码中对应为google::protobuf::Message的子类；

但只有参数信息是远远不够的，框架底层设计了RpcMessage消息类，包含了请求id，服务名、方法名等信息。
所以请求message将被序列化到RpcMessage的数据字段；这一步称为message到frame的转换，但这样还是不能够网络发送.

最后，需要对RpcMessage序列化，并添加4字节长度前缀，这一步称为frame到bytes的转换。
现在，请求可以从网络发送了，而且rpc服务器将能够解析请求，并定位到具体的方法调用。
注意，目前暂缺bytes到bytes的转换。比如，如果在发送之前想对数据加密或压缩呢？这个转换功能是非常容易添加的。

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
        // 出于示例，这里省略了对解码的讨论
    }
```

只需要在new客户端stub的时候设置一下channel回调即可:
```cpp
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


## 文档待补全
