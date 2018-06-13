#ifndef BERT_CONFIGPARSER_H
#define BERT_CONFIGPARSER_H

#include <map>
#include <string>
#include <sstream>

#ifdef CONFIG_DEBUG
#include <iostream>
#endif

namespace ananas
{

class ConfigParser
{
public:
    bool Load(const char* FileName);

    template <typename T>
    T GetData(const char* key, const T& default_ = T()) const;

#ifdef CONFIG_DEBUG
    void Print()
    {
        std::cout << "//////////////////"<< std::endl;
        for (const auto& kv : data_)
        {
            std::cout << kv.first << ":" << kv.second << "\n";
        }
    }
#endif

private:
    typedef std::map<std::string, std::string> Data;
    
    Data data_;

    template <typename T>
    T _ToType(const std::string& data) const;
};


template <typename T>
inline T ConfigParser::_ToType(const std::string& data) const
{
    T t;
    std::istringstream os(data);
    os >> t;
    return t;
}

template <>
inline const char* ConfigParser::_ToType<const char* >(const std::string& data) const
{
    return data.c_str();
}

template <>
inline std::string ConfigParser::_ToType<std::string>(const std::string& data) const
{
    return data;
}


template <typename T>
inline T ConfigParser::GetData(const char* key, const T& default_) const
{
    auto it = data_.find(key);
    if (it == data_.end())
        return default_;

    return _ToType<T>(it->second);
}

} // end namespace ananas

#endif

