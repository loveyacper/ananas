# ananas
[![Build Status](https://travis-ci.org/loveyacper/ananas.svg?branch=master)](https://travis-ci.org/loveyacper/ananas)


A C++11 RPC framework and toolbox for server-side development.


<!-- vim-markdown-toc GFM -->
* [Requirements](#requirements)
* [Features](#features)
* [What's ananas](#whats-ananas)
    * [需要什么预备知识](需要什么预备知识)
    * [阅读ananas需要读的书](阅读ananas需要读的书)
* [源码目录结构](#源码目录结构)
    * [net](#net)
    * [future](#future)
    * [util](#util)
    * [protobuf_rpc](#protobuf_rpc)
    * [unittest](#unittest)
    * [coroutine](#coroutine)
    * [tests](#tests)
    * [examples](#examples)
* [文档待补全](#文档待补全)
    
<!-- vim-markdown-toc -->

## Requirements
* C++11
* Linux or MAC OS
* google protobuf
* zookeeper or redis

## Features
* Protobuf-based rpc.
* Pythonic generator style coroutine, see [Intro](coroutine/README.md).
* `Future` & `Promise`, see [future](future/README.md).
* Network(`udp`&`tcp`,`kqueue`&`epoll`), support ssl.
* Easy used and powerful timer, see [test](tests/test_timer/).
* Extremely high performance multi-thread logger, see [Intro](util/log/README.md).
* etc...

## What's ananas?
&ensp;&ensp;&ensp;&ensp;ananas是一个基于promise模式和google protobuf的RPC框架，目前由C++11实现，golang版本在计划中。
      
&ensp;&ensp;&ensp;&ensp;ananas并不是一个大而全的框架，出于教学目的，也秉着less is more的原则，它只包含RPC框架该有的部分，非常小巧，便于初学者学习。

&ensp;&ensp;&ensp;&ensp;ananas主要包括两个部分:基础部分是一个**reactor模式的多线程非阻塞网络库**，基于epoll或kqueue，和陈硕先生的muduo比较相似；ananas有一些部分参考了java netty的实现。如果你是javaer，看到了EventLoopGroup可能会心一笑。

&ensp;&ensp;&ensp;&ensp;另一个部分则是在网络库之上，融合`google protobuf`以及`ananas future`库实现了一套RPC框架，只提供了基于future的异步调用接口。可能有人会问，为什么不提供同步。最大的原因是，我不认为一个非阻塞的框架应该提供同步操作（无协程情况）。尽管netty的future提供了阻塞接口，但官方文档中明确指出不提倡使用。如果思考一下one eventloop per thread模型就明白了，同步很有可能造成死锁。另一个原因是，future接口已经非常易用，并没有将逻辑打的支离破碎，而且支持when-all、when-any、when-N并发模式，十分强大。

### 需要什么预备知识
   * 你需要较为了解socket API，对TCP有一定认识;
   * 对多线程有所了解即可，会正确使用互斥锁、条件变量；
   * 需要较为熟悉C++11，特别是`shared_ptr, weak_ptr, bind&function, move&forward, lambda`.
   强烈推荐《Effective modern C++》.

   &ensp;&ensp;对于初学者，建议先放弃rpc和future部分，只研究网络库的实现；
   
   &ensp;&ensp;对于水平更高一些的用户，建议完整学习网络库和rpc实现，如果对future感兴趣，也可以研究源码，但难度较高。

### 阅读ananas源码需要读的书
   * [Effective modern C++](https://www.amazon.cn/dp/B016OFO492)
     
     学习C++11只需要这一本书,请耐心多读几遍。

   * [Unix网络编程](https://www.amazon.cn/dp/B011S72JB6)
    
     被推荐烂了，书很厚，但只需要读一小半:
     
     第1-7章；基础，特别是要理解TCP的双工特性.  
     第14章；了解gather write/scatter read.  
     第16章；这是本书最最重要的一章。  
     第30章；  
     也就是说，这么厚的书，只需要读10章足矣，是否有信心了?

### 推荐的书
   * [Netty in action](https://book.douban.com/subject/24700704/)
     
     各种语言的网络库千千万，netty是其中翘楚。
 
## 源码目录结构
  整个ananas的核心源码在一万行以内，其中net不足四千行，rpc部分约两千多行，只实现必不可少的部分，便于学习。
  ### net
  reactor模式的多线程非阻塞网络库，基于epoll和kqueue，one-eventloop-per-thread模型，支持TCP/UDP/Timer等功能。
  ### future
  受folly的future库启发而来，是ananas rpc的核心。极大方便了异步编程，支持链式回调，超时回调，when-all,when-any,when-N等模式。
  ### util
  基础工具库，主要有timer/delegate/defer/ThreadPool等功能，详见[util说明](util/README.md)
  ### protobuf_rpc
  基于protobuf和future的异步RPC库，可自定义协议；支持名字服务、轮转负载均衡、容错。
  ### unittest
  一个非常简单的单元测试类
  ### coroutine
  与python generator很相似的协程，使用swapcontext系统调用实现，只支持linux系统。
  ### tests
  一些测试代码
  ### examples
  一些用例代码
  

