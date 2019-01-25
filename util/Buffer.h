
#ifndef BERT_BUFFER_H
#define BERT_BUFFER_H

#include <cstring>
#include <memory>
#include <list>

///@file Buffer.h
namespace ananas {

///@brief A simple buffer with memory management like STL's vector<char>
class Buffer {
public:
    Buffer() :
        readPos_(0),
        writePos_(0),
        capacity_(0) {
    }

    Buffer(const void* data, size_t size) :
        readPos_(0),
        writePos_(0),
        capacity_(0) {
        PushData(data, size);
    }

    Buffer(const Buffer& ) = delete;
    void operator = (const Buffer& ) = delete;

    Buffer(Buffer&& ) ;
    Buffer& operator = (Buffer&& ) ;

    std::size_t PushDataAt(const void* data, std::size_t size, std::size_t offset = 0);
    std::size_t PushData(const void* data, std::size_t size);
    void Produce(std::size_t bytes) {
        writePos_ += bytes;
    }

    std::size_t PeekDataAt(void* pBuf, std::size_t size, std::size_t offset = 0);
    std::size_t PopData(void* pBuf, std::size_t size);
    void Consume(std::size_t bytes);

    char* ReadAddr()  {
        return &buffer_[readPos_];
    }
    char* WriteAddr() {
        return &buffer_[writePos_];
    }

    bool IsEmpty() const {
        return ReadableSize() == 0;
    }
    std::size_t ReadableSize() const {
        return writePos_ - readPos_;
    }
    std::size_t WritableSize() const {
        return capacity_ - writePos_;
    }
    std::size_t Capacity() const {
        return capacity_;
    }

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


struct BufferVector {
    static constexpr int kMinSize = 1024;

    typedef std::list<Buffer> BufferContainer;

    typedef BufferContainer::const_iterator const_iterator;
    typedef BufferContainer::iterator iterator;
    typedef BufferContainer::value_type value_type;
    typedef BufferContainer::reference reference;
    typedef BufferContainer::const_reference const_reference;

    BufferVector() {
    }

    BufferVector(Buffer&& first) {
        Push(std::move(first));
    }

    bool Empty() const {
        return buffers.empty();
    }

    std::size_t TotalBytes() const {
        return totalBytes;
    }

    void Clear() {
        buffers.clear();
        totalBytes = 0;
    }

    void Push(Buffer&& buf) {
        totalBytes += buf.ReadableSize();
        if (_ShouldMerge()) {
            auto& last = buffers.back();
            last.PushData(buf.ReadAddr(), buf.ReadableSize());
        } else {
            buffers.push_back(std::move(buf));
        }
    }

    void Push(const void* data, size_t size) {
        totalBytes += size;
        if (_ShouldMerge()) {
            auto& last = buffers.back();
            last.PushData(data, size);
        } else {
            buffers.push_back(Buffer(data, size));
        }
    }

    void Pop() {
        totalBytes -= buffers.front().ReadableSize();
        buffers.pop_front();
    }

    iterator begin() {
        return buffers.begin();
    }
    iterator end() {
        return buffers.end();
    }

    const_iterator begin() const {
        return buffers.begin();
    }
    const_iterator end() const {
        return buffers.end();
    }

    const_iterator cbegin() const {
        return buffers.cbegin();
    }
    const_iterator cend() const {
        return buffers.cend();
    }

    BufferContainer buffers;
    size_t totalBytes {0};

private:
    bool _ShouldMerge() const {
        if (buffers.empty()) {
            return false;
        } else {
            const auto& last = buffers.back();
            return last.ReadableSize() < kMinSize;
        }
    }
};

struct Slice {
    const void* data;
    size_t len;

    explicit
    Slice(const void* d = nullptr, size_t l = 0) :
        data(d),
        len(l)
    {}
};

struct SliceVector {
private:
    typedef std::list<Slice> Slices;

public:

    typedef Slices::const_iterator const_iterator;
    typedef Slices::iterator iterator;
    typedef Slices::value_type value_type;
    typedef Slices::reference reference;
    typedef Slices::const_reference const_reference;

    iterator begin() {
        return slices.begin();
    }
    iterator end() {
        return slices.end();
    }

    const_iterator begin() const {
        return slices.begin();
    }
    const_iterator end() const {
        return slices.end();
    }

    const_iterator cbegin() const {
        return slices.cbegin();
    }
    const_iterator cend() const {
        return slices.cend();
    }

    bool Empty() const {
        return slices.empty();
    }

    void PushBack(const void* data, size_t len) {
        slices.push_back(Slice(data, len));
    }

private:
    Slices slices;
};

} // end namespace ananas

#endif

