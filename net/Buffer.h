
#ifndef BERT_BUFFER_H
#define BERT_BUFFER_H

#include <cstring>
#include <vector>

namespace ananas
{

class Buffer
{
public:
    Buffer() :
        readPos_(0),
        writePos_(0)
    {
    }

    std::size_t PushDataAt(const void* pData, std::size_t nSize, std::size_t offset = 0);
    std::size_t PushData(const void* pData, std::size_t nSize);
    void Produce(std::size_t bytes) {   writePos_ += bytes; }

    std::size_t PeekDataAt(void* pBuf, std::size_t nSize, std::size_t offset = 0);
    std::size_t PeekData(void* pBuf, std::size_t nSize);
    void Consume(std::size_t bytes) {   readPos_  += bytes; }

    char* ReadAddr()  {  return &buffer_[readPos_];  }
    char* WriteAddr() {  return &buffer_[writePos_]; }

    bool IsEmpty() const { return ReadableSize() == 0; }
    std::size_t ReadableSize() const {  return writePos_ - readPos_;  }
    std::size_t WritableSize() const {  return buffer_.size() - writePos_;  }

    void Shrink(bool tight = false);
    void Clear();
    void Swap(Buffer& buf);

    void AssureSpace(std::size_t size);

    static const std::size_t  kMaxBufferSize;
    static const std::size_t  kWaterMark;
private:

    std::size_t readPos_;
    std::size_t writePos_;
    std::vector<char>  buffer_;
};

} // end namespace ananas

#endif

