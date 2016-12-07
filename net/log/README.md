# Multi-thread log

## Requirements
* C++11
* Linux or OS X

## Performance
O2优化,

单线程写100万条日志600ms左右,多线程速度可提高到每100万日志300-400ms左右

即每秒300w+

但线程过多可能会退化到单线程的水平.

## Rationale

每个生产者线程在格式化日志数据的时候是不需要锁的，因为访问的是thread-local数据.

每个生产者线程有独立的输出buffer，通过thread id来索引.

唯一的IO线程，将buffer输出到mmap的日志文件中.
