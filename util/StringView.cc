#include <cassert>
#include <cstring>
#include <algorithm>
#include "StringView.h"

namespace ananas
{
    
constexpr StringView::size_type StringView::npos;

StringView::StringView() :
    StringView(nullptr, 0)
{
}

StringView::StringView(const char* p) :
    StringView(p, strlen(p))
{
}

StringView::StringView(const std::string& s) :
    StringView(s.data(), s.size())
{
}


StringView::StringView(const char* p, size_t l) :
    data_(p),
    len_(l)
{
}


const char& StringView::operator[](int index) const &
{
    assert (index >= 0 && index < len_);
    return data_[index];
}

StringView::const_pointer StringView::Data() const
{
    return data_;
}

StringView::const_reference StringView::Front() const
{
    assert (len_ > 0);
    return data_[0];
}

StringView::const_reference StringView::Back() const
{
    assert (len_ > 0);
    return data_[len_-1];
}

StringView::const_iterator StringView::begin() const
{
    return data_;
}

StringView::const_iterator StringView::end() const
{
    return data_ + len_;
}

StringView::size_type StringView::Size() const
{
    return len_;
}

bool StringView::Empty() const
{
    return len_ == 0;
}

void StringView::RemovePrefix(size_t n)
{
    data_ += n;
    len_ -= n;
}

void StringView::RemoveSuffix(size_t n)
{
    len_ -= n;
}

void StringView::Swap(StringView& other)
{
    if (this != &other)
    {
        std::swap(this->data_, other.data_);
        std::swap(this->len_, other.len_);
    }
}

StringView StringView::Substr(size_type pos, size_type count) const
{
    return StringView(data_ + pos, std::min(count, Size() - count));
}

std::string StringView::ToString() const
{
    return std::string(data_, len_);
}


bool operator==(const StringView& a, const StringView& b)
{
    return a.Size() == b.Size() &&
           strncmp(a.Data(), b.Data(), a.Size()) == 0;
}

bool operator!=(const StringView& a, const StringView& b)
{
    return !(a == b);
}

bool operator<(const StringView& a, const StringView& b)
{
    int res = strncmp(a.Data(), b.Data(), std::min(a.Size(), b.Size()));
    if (res != 0)
        return res < 0;

    return a.Size() < b.Size();
}

bool operator>(const StringView& a, const StringView& b)
{
    return !(a<b || a==b);
}

bool operator<=(const StringView& a, const StringView& b)
{
    return !(a > b);
}

bool operator>=(const StringView& a, const StringView& b)
{
    return !(a < b);
}

} // end namespace ananas
 

