
#include "Buffer.h"
#include <iostream>
#include <limits>
#include <cassert>

namespace ananas
{

inline static std::size_t RoundUp2Power(std::size_t size)
{
    if (size == 0)
        return 0;

    std::size_t roundUp = 1;
    while (roundUp < size)
        roundUp *= 2;

    return roundUp;
}


const std::size_t Buffer::kMaxBufferSize = std::numeric_limits<std::size_t>::max() / 2;
const std::size_t Buffer::kHighWaterMark = 1 * 1024;
const std::size_t Buffer::kDefaultSize = 64;

std::size_t Buffer::PushData(const void* data, std::size_t size)
{
    std::size_t bytes = PushDataAt(data, size);
    Produce(bytes);

    assert (bytes == size);

    return bytes;
}

std::size_t Buffer::PushDataAt(const void* data, std::size_t size, std::size_t offset)
{
    if (!data || size == 0)
        return 0;

    if (ReadableSize() + size + offset >= kMaxBufferSize)
        return 0; // overflow

    AssureSpace(size + offset);

    assert (size + offset <= WritableSize());

    ::memcpy(&buffer_[writePos_ + offset], data, size);
    return  size;
}

std::size_t Buffer::PopData(void* buf, std::size_t size)
{
    std::size_t bytes = PeekDataAt(buf, size);
    Consume(bytes);

    return bytes;
}
    
void Buffer::Consume(std::size_t bytes)
{
    assert (readPos_ + bytes <= writePos_);

    readPos_ += bytes;
    if (IsEmpty())
        Clear();
}

std::size_t Buffer::PeekDataAt(void* buf, std::size_t size, std::size_t offset)
{
    const std::size_t dataSize = ReadableSize();
    if (!buf ||
         size == 0 ||
         dataSize <= offset)
        return 0;

    if (size + offset > dataSize)
        size = dataSize - offset; // truncate

    ::memcpy(buf, &buffer_[readPos_ + offset], size);
    return size;
}


void Buffer::AssureSpace(std::size_t needsize)
{
    if (WritableSize() >= needsize)
        return;

    const size_t dataSize = ReadableSize();
    const size_t oldCap = capacity_;

    while (WritableSize() + readPos_ < needsize)
    {
        if (capacity_ < kDefaultSize)
        {
            capacity_ = kDefaultSize;
        }
        else if (capacity_ <= kMaxBufferSize)
        {
            const auto newCapcity = RoundUp2Power(capacity_);
            if (capacity_ < newCapcity)
                capacity_ = newCapcity;
            else
                capacity_ = 2 * newCapcity;
        }
        else 
        {
            assert (false);
        }
    }

    if (oldCap < capacity_)
    {
        std::unique_ptr<char []> tmp(new char[capacity_]);

        if (dataSize != 0)
            memcpy(&tmp[0], &buffer_[readPos_], dataSize);

        buffer_.swap(tmp);
        //std::cout << " expand to " << capacity_ << ", and data size " << dataSize << std::endl;
    }
    else
    {
        assert (readPos_ > 0);

        ::memmove(&buffer_[0], &buffer_[readPos_], dataSize);
        std::cout << " move from " << readPos_ << ", and dataSize " << dataSize << std::endl;
    }

    readPos_ = 0;
    writePos_ = dataSize;
}


void Buffer::Shrink()
{
    if (IsEmpty())
    {
        Clear();
        capacity_ = 0;
        buffer_.reset();
        return;
    }

    std::size_t oldCap = capacity_;
    std::size_t dataSize = ReadableSize();
    if (dataSize > oldCap / 2)
        return;

    std::size_t newCap = RoundUp2Power(dataSize);

    std::unique_ptr<char []> tmp(new char[newCap]);
    memcpy(&tmp[0], &buffer_[readPos_], dataSize);
    buffer_.swap(tmp);
    capacity_ = newCap;

    readPos_  = 0;
    writePos_ = dataSize;

    std::cout << oldCap << " shrink to " << capacity_ << std::endl;
}

void Buffer::Clear()
{
    readPos_ = writePos_ = 0; 
}


void Buffer::Swap(Buffer& buf)
{
    std::swap(readPos_, buf.readPos_);
    std::swap(writePos_, buf.writePos_);
    std::swap(capacity_, buf.capacity_);
    buffer_.swap(buf.buffer_);
}
    
Buffer::Buffer(Buffer&& other)
{
    _MoveFrom(std::move(other));
}

Buffer& Buffer::operator= (Buffer&& other) 
{
    return _MoveFrom(std::move(other));
}

Buffer& Buffer::_MoveFrom(Buffer&& other)
{
    if (this != &other)
    {
        this->readPos_ = other.readPos_;
        this->writePos_ = other.writePos_;
        this->capacity_ = other.capacity_;
        this->buffer_ = std::move(other.buffer_);

        other.Clear();
        other.capacity_ = 0;
    }

    return *this;
}

} // end namespace ananas

