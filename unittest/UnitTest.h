#ifndef BERT_UNITTEST_H
#define BERT_UNITTEST_H

#include <string>
#include <sstream>
#include <vector>

class  UnitTestBase
{
public:
    friend class MsgHelper;

    UnitTestBase();
    virtual ~UnitTestBase() {}
    // stack only, no need virtual destructor, but the warning...
    
    const std::string& GetName() const  {  return m_name;  }
    
    virtual void Run() = 0;

    bool  IsFine() const    {  return m_errors.empty();  }
    void  Print()  const;

    template <typename T>
    UnitTestBase& operator << (const T &  t);

protected:
    UnitTestBase& SetInfo(const std::string& exprInfo, bool pass = true, bool abort = false);
    std::string   m_name;

private:
    void    FlushError();
    
    bool    m_pass;
    bool    m_abort;

    std::string     m_expr;
    std::vector<std::string>    m_errors;
    
private:
    UnitTestBase(const UnitTestBase& );
    UnitTestBase& operator= (const UnitTestBase& );

    void* operator new(std::size_t ); // stack only
};

template <typename T>
inline UnitTestBase& UnitTestBase::operator<< (const T &  t)
{
    if (!m_pass)
    {
        std::ostringstream  str;
        str << t;
        m_expr += str.str();
    }

    return *this;
}

class MsgHelper 
{
public:
    void operator=(UnitTestBase& test)
    {
        test.FlushError();
    }
};



#define   TEST_CASE(name)                           \
    class UnitTestBase##name: public UnitTestBase   \
    {                                               \
    public:                                         \
        UnitTestBase##name() {                      \
            m_name = #name;                         \
        }                                           \
        virtual void Run();                         \
    } test_##name##_obj;                            \
    void    UnitTestBase##name::Run()


#define EXPECT_TRUE(expr)   \
    MsgHelper()=((expr) ? SetInfo("'"#expr"'", true) : SetInfo("'"#expr"'", false))

#define EXPECT_FALSE(expr)  \
    EXPECT_TRUE(!(expr))

#define ASSERT_TRUE(expr)   \
    MsgHelper()=((expr) ? SetInfo("'"#expr"'", true) : SetInfo("'"#expr"' ", false, true))

#define ASSERT_FALSE(expr)  \
    ASSERT_TRUE(!(expr))



class UnitTestManager
{
public:
    static  UnitTestManager&    Instance();

    void    AddTest(UnitTestBase* test);
    void    Clear();
    void    Run();
private:
    UnitTestManager() {}
    
    std::vector<UnitTestBase* > m_tests;
};

#define RUN_ALL_TESTS   UnitTestManager::Instance().Run

#endif

