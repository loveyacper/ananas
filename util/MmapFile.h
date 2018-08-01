#ifndef BERT_MMAPFILE_H
#define BERT_MMAPFILE_H

#include <string>

namespace ananas {
namespace internal {

class OMmapFile {
public:
    OMmapFile();
    ~OMmapFile();

    bool Open(const std::string& file, bool bAppend = true);
    bool Open(const char* file, bool bAppend = true);
    void Close();
    bool Sync();

    void Truncate(std::size_t size);

    void Write(const void* data, std::size_t len);
    template <typename T>
    void Write(const T& t);

    std::size_t Offset() const {
        return offset_;
    }
    bool IsOpen() const;

private:
    bool _MapWriteOnly();
    void _ExtendFileSize(std::size_t size);
    void _AssureSpace(std::size_t size);

    int file_;
    char* memory_;
    std::size_t offset_;
    std::size_t size_;
    std::size_t syncPos_;
};


template <typename T>
inline void OMmapFile::Write(const T& t) {
    this->Write(&t, sizeof t);
}

} // end namespace internal

} // end namespace ananas

#endif

