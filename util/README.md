# ananas util 工具库

=========

## 1.ThreadPool
* ananas线程池

  采用C++11实现，利用了变长模板参数，对投递的任务函数签名没有任何限制，不像传统的线程实现必须接受
  void func(void\* ）的签名。同时结合Future，可以方便的注册回调，前面已经叙述过。ananas线程池可以设置最大线程
  数量和最小线程数量。在任务比较多的时候，线程数会增加，但不会超过你设置的线程池大小；在任务比较少的时候，ananas
  会自动回收多余的线程

  ```cpp
  int getMoney(const std::string& name);
  std::string getInfo(int year, const std::string& city);

  auto& pool = ananas::ThreadPool::Instance();

  pool.Execute(getMoney, "mahuateng")
      .Then([](int money) {
          cout << "mahuateng has money " << money << endl;
      });

  pool.Execute(getInfo, 2017, "shanghai")
      .Then([](const std::string& info) {
          cout << info << endl;
      });

  // 在某个线程内睡眠10秒
  pool.Execute([] (int n) { sleep(n); },  10);
  ```

## 2.Defer
* ananas Defer

  顾名思义，听起来有点像golang的defer。是的，但功能更强大一点：作用域可以比函数作用域更小.

  ```cpp
  void Foo() 
  {
      MYSQL mysql = mysql_open(xxx);

      // 当函数退出时，关闭连接
      ANANAS_DEFER {
          mysql_close(mysql);
      };

      if (mysql.select(xxx) == 0)
          throw XXXExcetpion(); // 无论怎样都不会泄露mysql句柄

      if (!mysql.Insert(yyy))
          return;

      {
          // 块作用域
          // 如果没有提前返回，这句话会比关闭句柄提前执行
          ANANAS_DEFER {
              printf("defer test\n");
          };
      }
  }
  ```

## 3.Delegate
* ananas Delegate

  觉得C#的委托非常有用，解耦的一大利器，使用100行C++11代码做了简单模拟

  ```cpp

  // 三个函数的定义
  void Inc(int& n)
  {
      ++n;
  }

  void Print(int& n)
  {
      cout << n << endl;
  }

  class Test
  {
  public:
      void MInc(int& n)
      {
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

## 4.Logger
* ananas logger

  性能极高的多线程日志(每秒写入可超过300万条),详见[Intro](log/README.md)

## 5.Timer
* ananas timer

  非常好用的定时器,支持任意的签名函数,支持次数定制.由于Time不直接暴露,而是与Eventloop整合,
  这里我就不单独介绍了.

## 6.Buffer
* 缓冲区实现

  很简单但也很高效的Buffer实现,主要用于TCP连接的收发缓冲区.实现机制类似于vector的内存管理。
  与流行的muduo库中Buffer不同的是,ananas使用了裸的字节缓存区,避免了vector<char>的resize方法带来的memset浪费.
