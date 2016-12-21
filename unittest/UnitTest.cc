#include "UnitTest.h"
#include <iostream>

enum Color
{
    Color_red       =  1,
    Color_green         ,
    Color_yellow        ,
    Color_normal        ,
    Color_blue          ,
    Color_purple        ,
    Color_white         ,
    Color_max           ,
};

void  SetColor(Color c)
{
    static const char* colors[Color_max] = {
        "",
        "\033[1;31;40m",
        "\033[1;32;40m",
        "\033[1;33;40m",
        "\033[0m",
        "\033[1;34;40m",
        "\033[1;35;40m",
        "\033[1;37;40m",
    };

    fprintf(stdout, "%s", colors[c]);
}

UnitTestBase::UnitTestBase() : m_pass(true), m_abort(false)
{
    UnitTestManager::Instance().AddTest(this);
}
    
    
UnitTestBase& UnitTestBase::SetInfo(const std::string& exprInfo, bool pass, bool abort)
{
    m_pass  = pass;
    m_abort = abort;
    m_expr  = (m_pass ? "[passed]: " : "[failed]: ") + exprInfo;
    return *this;
}


void  UnitTestBase::Print() const
{
    SetColor(Color_red);
    for (const auto& err : m_errors)
    {
        std::cout << err << std::endl;
    }
    SetColor(Color_normal);
}

    
void UnitTestBase::FlushError()
{
    if (m_pass)
    {
        SetColor(Color_green);
        std::cout << m_expr << std::endl;
        return;
    }

    m_errors.push_back(m_expr);
    if (m_abort)
    {
        Print();
        ::abort();
    }
}


// test mgr
UnitTestManager& UnitTestManager::Instance()
{
    static UnitTestManager mgr;
    return mgr;
}

void UnitTestManager::AddTest(UnitTestBase* test)
{
    m_tests.push_back(test);
}

void UnitTestManager::Clear()
{
    m_tests.clear();
}

void UnitTestManager::Run()
{
    std::size_t  pass = 0;
    std::size_t  fail = 0;

    for (auto ut : m_tests)
    {
        ut->Run();
        if (ut->IsFine())
        {
            ++ pass;

            SetColor(Color_white);
            std::cout << "ALL PASSED! " << ut->GetName() << std::endl;
        }
        else
        {
            ++ fail;

            ut->Print();
            SetColor(Color_purple);
            std::cout << "FAILED! " << ut->GetName() << std::endl;
        }
    }

    Clear();

    SetColor(fail == 0 ? Color_blue: Color_yellow);
    std::cout << (pass + fail) << " cases: "
              << pass << " passed, "
              << fail << " failed\n";
    SetColor(Color_normal);
}

#define HELLO_TEST  0

#if HELLO_TEST
bool f(bool b)
{
    return b;
}

TEST_CASE(test1)
{
    EXPECT_TRUE(5 != 3) << "fuck, why 5 != 3 failed ? ";
    EXPECT_FALSE(0 != 0);
    EXPECT_FALSE(3 == 2) << "fuck, why 3 == 2 passed? ";
}

TEST_CASE(test2)
{
    ASSERT_FALSE(2 != 2) << "fuck, why 2 != 2 passed? ";
    ASSERT_TRUE(f(true)) << "fuck, why failed? ";

    EXPECT_TRUE(9 != 9) << " true raise error!";
    EXPECT_FALSE(0 == 0) << " raise error!";
    ASSERT_FALSE(2 == 2) << " raise abort!";
    ASSERT_FALSE(-1 == 0) << "fuck, why -1 == 0 passed? ";
}
#endif

