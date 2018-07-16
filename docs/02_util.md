# ananas util 工具库

## 简介

  顾名思义，util是工具库，属于最基础的设施。主要包含了线程池、日志、定时器等功能。

## 1. ANANAS_DEFER

  接触过golang的朋友应该都使用过defer。尽管C++没有直接提供defer，由于强大的RAII和lambda，我只用了几十行代码就完美实现了DEFER。

  golang的defer只能是函数作用域，而ANANAS_DEFER则可细化到块作用域。在ananas代码中很多地方都使用了ANANAS_DEFER。

  例如在`net/EventLoop.cc`的Loop函数中，退出点有好几个，但无论从哪里退出，我们都希望在退出前执行定时器：

  ```cpp
  ANANAS_DEFER
  {
      timers_.Update();
  };
  ```

  我们只需要在ANANAS_DEFER后使用大括号将要执行的语句包围起来，以分号结束即可。RAII机制确保语句块将在作用域失效前执行。golang中更常用的是关闭句柄、释放资源等操作，但C++一般将资源管理交给了RAII，所以DEFER中一般不需要这些操作。

  多个ANANAS_DEFER块之前是类似栈一样的先进后出的执行关系，例如：
  ```cpp
  int a = 2;
  int b = 3;

  ANANAS_DEFER {
      a = 0; // the last
  };

  ANANAS_DEFER {
      std::cout << (a + b) << std::endl; // second
  };

  ANANAS_DEFER {
      b = 0; // first
  };
  ```
  与C++栈上对象的析构顺序相同，最后一个语句块b=0最先执行，然后执行第二个语句块打印a+b的值，最后执行a=0；因此程序结果输出是2。

  最后看一个伪代码例子，解释就放在代码注释中：
  ```cpp
  bool Foo() {
      MYSQL mysql = mysql_open(xxx);

      // mysql_close并不是现在就执行，而是当函数退出时执行，关闭mysql连接
      ANANAS_DEFER {
          mysql_close(mysql);
      };

      if (mysql.select(xxx) == -1) {
          throw MysqlExcetpion("query failed");
          // 就算抛出异常, mysql_close也一定执行，不会句柄泄露
      }

      if (!mysql.Insert(yyy))
          return false;

      return true;
  }
  ```

## Delegate

  觉得C#的委托非常有用，解耦的一大利器，使用100行C++11代码做了简单模拟

  ```cpp
  // 三个函数的定义
  void Inc(int& n) {
      ++n;
  }

  void Print(int& n) {
      cout << n << endl;
  }

  class Test {
  public:
      void MInc(int& n) {
          ++n;
      }
  };
    
  // 测试委托
  int n = 0;
  ananas::Delegate<void (int& )> cb;

  cb += Inc;
  cb += Print;
  cb(n);  // 先执行Inc,再执行print打印1
    
  cb -= Inc;
  cb(n);  // Inc函数被从委托中删除了,现在只执行print,还是打印1

  Test obj;
  cb += std::bind(&Test::MInc, &obj, std::placeholders::_1); //也可以注册成员函数
    
  // 注册lambda也没问题,只要签名一致
  cb += [](int& b) { ++b; };
  ```
  委托的一个典型应用场景是在观察者模式中，subject使用委托管理observer的注册函数。当有事件发生，直接投递给委托执行。
  Delegate有一点使用注意项：参数不要使用rvalue reference。因为委托是函数的集合，除了第一个函数，后续函数将得到空的参数输入。所以一般来说，函数签名接受的是参数的const reference。


* 还没写完，抽时间再写

