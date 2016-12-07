#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <iostream>
#include <unistd.h>


#include "MmapFile.h"

namespace ananas
{
namespace internal
{

static const size_t kDefaultSize = 1 * 1024 * 1024;

static const int   kInvalidFile = -1;
static char* const kInvalidAddr = reinterpret_cast<char*>(-1);


// OMmapFile
OMmapFile::OMmapFile() : file_(kInvalidFile),
    memory_(kInvalidAddr),
    offset_(0),
    size_(0),
    syncPos_(0)
{
}

OMmapFile::~OMmapFile()
{
    Close();
}

void OMmapFile::_ExtendFileSize(size_t size)
{
    assert(file_ != kInvalidFile);

    if (size > size_)
        Truncate(size);
}

bool OMmapFile::Open(const std::string& file, bool bAppend)
{
    return Open(file.c_str(), bAppend);
}

bool OMmapFile::Open(const char* file, bool bAppend)
{
    Close();

    file_ = ::open(file, O_RDWR | O_CREAT | (bAppend ? O_APPEND : 0), 0644);

    if (file_ == kInvalidFile)
    {
        char err[128];
        snprintf(err, sizeof err - 1, "OpenWriteOnly %s failed\n", file);
        perror(err);
        assert (false);
        return false;
    }

    if (bAppend)
    {
        struct stat st;
        fstat(file_, &st);
        size_ = std::max<size_t>(kDefaultSize, st.st_size);
        offset_ = st.st_size;
    }
    else
    {
        size_ = kDefaultSize;
        offset_ = 0;
    }

    int ret = ::ftruncate(file_, size_);
    assert (ret == 0);

    return _MapWriteOnly();
}

void  OMmapFile::Close()
{
    if (file_ != kInvalidFile)
    {
        ::munmap(memory_, size_);
        ::ftruncate(file_, offset_);
        ::close(file_);

        file_ = kInvalidFile;
        size_ = 0;
        memory_ = kInvalidAddr;
        offset_ = 0;
        syncPos_ = 0;
    }
}

bool    OMmapFile::Sync()
{
    if (file_ == kInvalidFile)
        return false;

    if (syncPos_ >= offset_)
        return false;

    ::msync(memory_ + syncPos_, offset_ - syncPos_, MS_SYNC);
    syncPos_ = offset_;
    
    return true;
}

bool OMmapFile::_MapWriteOnly()
{
    if (size_ == 0 || file_ == kInvalidFile)
    {
        assert (false);
        return false;
    }

    memory_ = (char*)::mmap(0, size_, PROT_WRITE, MAP_SHARED, file_, 0);

    assert (memory_ > 0);
    return (memory_ != kInvalidAddr);
}

void OMmapFile::Truncate(std::size_t  size)
{
    if (size == size_)
        return;

    if (memory_ > 0)
    {
        //int ret = ::munmap(memory_, size_);
        //assert (ret == 0);
    }

    size_ = size;
    int ret = ::ftruncate(file_, size_);
    assert (ret == 0);

    if (offset_> size_)
        offset_ = size_;

    _MapWriteOnly();
}

bool OMmapFile::IsOpen() const
{
    return  file_ != kInvalidFile;
}

// consumer
void OMmapFile::Write(const void* data, size_t len)
{
    _AssureSpace(len);

    assert(memory_ > 0);
    assert (offset_ + len <= size_);

    ::memcpy(memory_ + offset_, data, len);
    offset_ += len;
    assert(offset_ <= size_);
}

void OMmapFile::_AssureSpace(size_t len)
{
    size_t newSize = size_;

    while (offset_ + len > newSize)
    {
        if (newSize == 0)
            newSize = kDefaultSize;
        else
            newSize *= 2;
    }

    _ExtendFileSize(newSize);
}

} // end namespace internal

} // end namespace ananas

