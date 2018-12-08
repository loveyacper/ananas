# ananas util 工具库

## 简介

  顾名思义，util是工具库，属于最基础的设施。主要包含了线程池、日志、定时器等功能。

## ANANAS_DEFER

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

      return mysql.Insert(yyy);
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


## ThreadPool
* ananas线程池

  利用了变长模板参数，对投递的任务函数签名没有任何限制，不像传统的线程实现必须接受
  void func(void\* ）的签名。同时结合Future，可以方便的注册回调，前面已经叙述过。ananas线程池可以设置最大线程
  数量和最小线程数量。在任务比较多的时候，线程数会增加，但不会超过你设置的线程池大小；在任务比较少的时候，ananas
  会自动回收多余的线程

  ```cpp
  extern int getMoney(const std::string& name);
  extern std::string getInfo(int year, const std::string& city);

  ananas::ThreadPool pool;

  pool.Execute(getMoney, "mahuateng")
      .Then([](int money) {
          cout << "mahuateng has money " << money << endl;
      });

  pool.Execute(getInfo, 2017, "shanghai")
      .Then([](const std::string& info) {
          cout << info << endl;
      });

  // 在某个线程内睡眠10秒
  pool.Execute(::sleep,  10);
  ```

## Timer

* ananas定时器

  定时器是一个非常基础必要的功能。EventLoop暴露定时器习惯接口，TimerMananger本身作为EventLoop的成员做真正的工作。
  这里以java定时器提供的其中四个接口为例，给出ananas对应的用法。

  ```cpp
  // 1. 在指定的时间点，执行任务一次
  // task可以是任意签名的函数，可以带任意参数
  loop.ScheduleAt(time, task, args...);


  // 2. 在经过指定的延迟时间后，执行任务一次
  // task可以是任意签名的函数，可以带任意参数
  loop.ScheduleAfter(delay, task, args...);


  // 3. 在经过指定的延迟时间后，执行任务，之后以固定的周期重复执行指定的任务。
  // ananas可以指定重复次数，在task执行指定次数后停止，也可以kForever不停止
  loop.ScheduleAfterWithRepeat<kForever>(delay, period, task, args...);


  // 4. 在指定的时间点，执行任务，之后以固定的周期重复执行指定的任务。
  // ananas可以指定重复次数，在task执行指定次数后停止，也可以kForever不停止
  loop.ScheduleAtWithRepeat<kForever>(delay, period, task, args...);
  ```

  和java不同的是，后面两个接口，提供了任务触发次数，java只允许是forever触发。
  另外，如果定时器某一次触发有延迟，后续的触发会catch up，这和java的scheduleAtFixedRate是一致的。


