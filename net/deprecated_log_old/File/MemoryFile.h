#ifndef BERT_MEMORYFILE_H
#define BERT_MEMORYFILE_H

#include <string>

class InputMemoryFile
{
public:
    InputMemoryFile();
   ~InputMemoryFile();

    bool Open(const char* file);
    void Close();

    const char* Read(std::size_t& len);
    template <typename T>
    T Read();
    void Skip(std::size_t len);

    bool IsOpen() const;

private:
    bool _MapReadOnly();

    int file_;
    char* pMemory_;
    std::size_t offset_;
    std::size_t size_;
};

template <typename T>
inline T  InputMemoryFile::Read()
{
    T res(*reinterpret_cast<T* >(pMemory_ + offset_));
    offset_ += sizeof(T);
    
    return res;
}


class OutputMemoryFile
{
public:
    OutputMemoryFile();
   ~OutputMemoryFile();

    bool Open(const std::string& file, bool bAppend = true);
    bool Open(const char* file, bool bAppend = true);
    void Close();
    bool Sync();

    void Truncate(std::size_t size);
    //!! if process terminated abnormally, erase the trash data
    void TruncateTailZero();

    void Write(const void* data, std::size_t len);
    template <typename T>
    void Write(const T& t);
    
    std::size_t Offset() const { return offset_; }
    bool IsOpen() const;

private:
    bool _MapWriteOnly();
    void _ExtendFileSize(std::size_t size);
    void _AssureSpace(std::size_t size);

    int file_;
    char* pMemory_;
    std::size_t offset_;
    std::size_t size_;
    std::size_t syncPos_;
};


template <typename T>
inline void OutputMemoryFile::Write(const T& t)
{
    this->Write(&t, sizeof t);
}

#endif

