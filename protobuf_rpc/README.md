# ananas protobuf rpc


<!-- vim-markdown-toc GFM -->
* [入门例子](#入门例子)
    * [编写proto](#编写proto)
    * [服务端](#服务端)
    * [客户端](#客户端)
    * [bootstrap](#bootstrap)
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

    // 发起调用
    Call<test::EchoResponse>("ananas.rpc.test.TestService",  // service name
                             "ToUpper",                      // method name
                             req)                            // request args
                            .Then(OnResponse);               // response handler

    // 其中OnResponse是注册的回调函数，定义如下：
    void OnResponse(ananas::Try<ananas::rpc::test::EchoResponse>&& response)
    {
        // Try类型表示一个值，也可能表示一个异常
        // 所以在访问Try时候，可能会throw
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

    // 设置名字服务地址，其实默认是redis，服务向redis注册服务名和endpoint
    // 客户端在Call之前，会自动访问redis得到此服务的endpoint列表，再发起真正的rpc调用
    server.SetNameServer("tcp://127.0.0.1:9900");
    // 服务启动，进入无限循环
    server.Start();
```
启动RPC客户端类似，只是把Service换成ServiceStub.

## 文档待补全
