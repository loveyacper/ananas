#include <vector>
#include <fstream>
#include "ConfigParser.h"

static const int  SPACE     = ' ';
static const int  TAB       = '\t';
static const int  NEWLINE   = '\n';
static const int  COMMENT   = '#';

static size_t SkipBlank(const char* data, size_t len, size_t off)
{
    while (++ off < len)
    {
        if (SPACE != data[off] && TAB != data[off])
        {
            -- off;
            break;
        }
    }

    return off;
}

namespace ananas
{

bool ConfigParser::Load(const char* FileName)
{
    std::fstream file;
    file.open(FileName, std::ios::in | std::ios::binary);
    if (!file)
        return false; // no such file

    file.seekg(0, std::ios::end);  
    const size_t size = file.tellg();
    file.seekg(0, std::ios::beg);  

    data_.clear();
    std::unique_ptr<char []> _(new char[size]);
    char* data = _.get();
    file.read(data, size);

    bool  bReadKey = true;
    std::string  key, value;
    key.reserve(64);
    value.reserve(64);

    size_t  off = 0;
    while (off < size)
    {
        switch (data[off])
        {
            case COMMENT:
                while (++ off < size)
                {
                    if (NEWLINE == data[off])
                    {
                        -- off;
                        break;
                    }
                }
                break;

            case NEWLINE:
                bReadKey = true;

                if (!key.empty())
                {
                    data_[key] = value;
                    
                    key.clear();
                    value.clear();
                }
                
                off = SkipBlank(data, size, off);
                break;
            
            case SPACE:
            case TAB:
                // 支持value中有空格
                if (bReadKey)
                {
                    bReadKey = false;
                    off = SkipBlank(data, size, off); // 跳过所有分界空格
                }
                else
                {
                    value += data[off];
                }
                break;
            
            case '\r':
                break;
            
            default:
                if (bReadKey)
                    key += data[off];
                else
                    value += data[off];

                break;
        }

        ++ off;
    }
    
    return true;
}

} // end namespace ananas

#ifdef CONFIG_DEBUG
int main()
{
    ConfigParser   csv;
    csv.Load("config");
    csv.Print();
	
    std::cout << "=====================" << std::endl;
}

#endif

