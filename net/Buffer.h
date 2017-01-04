
#ifndef BERT_BUFFER_H
#define BERT_BUFFER_H

#include <cstring>
#include <memory>
#include <list>

namespace ananas
{

class Buffer
{
public:
    Buffer() :
        readPos_(0),
        writePos_(0),
        capacity_(0)
    {
    }

    Buffer(const Buffer& ) = delete;
    void operator = (const Buffer& ) = delete;

    Buffer(Buffer&& ) ;
    Buffer& operator = (Buffer&& ) ;

    std::size_t PushDataAt(const void* data, std::size_t size, std::size_t offset = 0);
    std::size_t PushData(const void* data, std::size_t size);
    void Produce(std::size_t bytes) {   writePos_ += bytes; }

    std::size_t PeekDataAt(void* pBuf, std::size_t size, std::size_t offset = 0);
    std::size_t PopData(void* pBuf, std::size_t size);
    void Consume(std::size_t bytes);

    char* ReadAddr()  {  return &buffer_[readPos_];  }
    char* WriteAddr() {  return &buffer_[writePos_]; }

    bool IsEmpty() const { return ReadableSize() == 0; }
    std::size_t ReadableSize() const {  return writePos_ - readPos_;  }
    std::size_t WritableSize() const {  return capacity_ - writePos_;  }
    std::size_t Capacity() const {  return capacity_;  }

    void Shrink();
    void Clear();
    void Swap(Buffer& buf);

    void AssureSpace(std::size_t size);

    static const std::size_t  kMaxBufferSize;
    static const std::size_t  kHighWaterMark;
    static const std::size_t  kDefaultSize;

private:
    Buffer& _MoveFrom(Buffer&& );

    std::size_t readPos_;
    std::size_t writePos_;
    std::size_t capacity_;
    std::unique_ptr<char []>  buffer_;
};


struct BufferVector
{
    typedef std::list<Buffer> BufferContainer;

    typedef BufferContainer::const_iterator const_iterator;
    typedef BufferContainer::iterator iterator;
    typedef BufferContainer::value_type value_type;
    typedef BufferContainer::reference reference;
    typedef BufferContainer::const_reference const_reference;

    BufferVector()
    {
    }

    BufferVector(Buffer&& first)
    {
        PushBack(std::move(first));
    }

    std::size_t TotalBytes() const
    {
        size_t s = 0;
        for (const auto& buf : buffers)
            s += buf.ReadableSize();

        return s;
    }

    void PushBack(Buffer&& buf)
    {
        buffers.push_back(std::move(buf));
    }

    void PushFront(Buffer&& buf)
    {
        buffers.push_front(std::move(buf));
    }
    void PopBack()
    {
        buffers.pop_back();
    }

    void PopFront()
    {
        buffers.pop_front();
    }

    iterator begin() { return buffers.begin(); }
    iterator end() { return buffers.end(); }

    const_iterator begin() const { return buffers.begin(); }
    const_iterator end() const { return buffers.end(); }

    const_iterator cbegin() const { return buffers.cbegin(); }
    const_iterator cend() const { return buffers.cend(); }

private:
    BufferContainer buffers;
};


} // end namespace ananas

#endif

