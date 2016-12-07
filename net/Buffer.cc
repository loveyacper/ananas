
#include "Buffer.h"
#include <iostream>
#include <limits>
#include <cassert>
    
namespace ananas
{

const std::size_t Buffer::kMaxBufferSize = std::numeric_limits<std::size_t>::max() / 2;
const std::size_t Buffer::kWaterMark     = 1024;

std::size_t Buffer::PushData(const void* pData, std::size_t nSize)
{
    std::size_t bytes  = PushDataAt(pData, nSize);
    Produce(bytes);

    return bytes;
}

std::size_t Buffer::PushDataAt(const void* pData, std::size_t nSize, std::size_t offset)
{
    if (!pData || nSize == 0)
        return 0;

    if (ReadableSize() == kMaxBufferSize)
        return 0;

    AssureSpace(nSize + offset);

    assert (nSize + offset <= WritableSize());

   	::memcpy(&buffer_[writePos_ + offset], pData, nSize);
    return  nSize;
}

std::size_t Buffer::PeekData(void* pBuf, std::size_t nSize)
{
    std::size_t bytes  = PeekDataAt(pBuf, nSize);
    Consume(bytes);

    return bytes;
}

std::size_t Buffer::PeekDataAt(void* pBuf, std::size_t nSize, std::size_t offset)
{
    const std::size_t dataSize = ReadableSize();
    if (!pBuf ||
         nSize == 0 ||
         dataSize <= offset)
        return 0;

    if (nSize + offset > dataSize)
        nSize = dataSize - offset;

	::memcpy(pBuf, &buffer_[readPos_ + offset], nSize);

    return nSize;
}


void Buffer::AssureSpace(std::size_t nSize)
{
    if (nSize <= WritableSize())
        return;

    std::size_t maxSize = buffer_.size();

    while (nSize > WritableSize() + readPos_)
    {
        if (maxSize < 64)
            maxSize = 64;
        else if (maxSize <= kMaxBufferSize)
            maxSize <<= 1;
            //maxSize = (maxSize * 3) / 2;
        else 
            break;

        buffer_.resize(maxSize);
    }
        
    if (readPos_ > 0 && WritableSize() < kWaterMark)
    {
        std::size_t dataSize = ReadableSize();
        ::memmove(&buffer_[0], &buffer_[readPos_], dataSize);
        readPos_  = 0;
        writePos_ = dataSize;
    }
}

void Buffer::Shrink(bool tight)
{
    assert (buffer_.capacity() == buffer_.size());

    if (buffer_.empty())
    { 
        assert (readPos_ == 0);
        assert (writePos_ == 0);
        return;
    }

    std::size_t oldCap   = buffer_.size();
    std::size_t dataSize = ReadableSize();
    if (!tight && dataSize > oldCap / 2)
        return;

    std::vector<char>  tmp;
    tmp.resize(dataSize);
    memcpy(&tmp[0], &buffer_[readPos_], dataSize);
    tmp.swap(buffer_);

    readPos_  = 0;
    writePos_ = dataSize;

    std::cout << oldCap << " shrink to " << buffer_.size() << std::endl;
}

void Buffer::Clear()
{
    readPos_ = writePos_ = 0; 
}


void Buffer::Swap(Buffer& buf)
{
    buffer_.swap(buf.buffer_);
    std::swap(readPos_, buf.readPos_);
    std::swap(writePos_, buf.writePos_);
}

} // end namespace ananas

#if 0
int main()
{
    Buffer    buf;
    std::size_t ret = buf.PushData("hello", 5);
    assert (ret == 5);

    char tmp[10];
    ret = buf.PeekData(tmp, sizeof tmp);
    assert(ret == 5);
    assert(tmp[0] == 'h');

    assert(buf.IsEmpty());

    ret = buf.PushData("world", 5);
    assert (ret == 5);
    ret = buf.PushData("abcde", 5);
    assert (ret == 5);
    ret = buf.PeekData(tmp, 5);
    assert(tmp[0] == 'w');

    buf.Clear();
    buf.Shrink();

#if 1
    ret = buf.PeekData(tmp, 5);
    if (ret == 5)
    {
        assert(tmp[0] == 'a');
        assert(tmp[1] == 'b');
    }
#endif
    buf.Shrink();

    return 0;
}
#endif

