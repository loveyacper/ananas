#include "UnitTest.h"
#include <iostream>

enum Color {
    Color_red       =  1,
    Color_green,
    Color_yellow,
    Color_normal,
    Color_blue,
    Color_purple,
    Color_white,
    Color_max,
};

void SetColor(Color c) {
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

UnitTestBase::UnitTestBase() :
    pass_(true),
    abort_(false) {
    UnitTestManager::Instance().AddTest(this);
}


UnitTestBase& UnitTestBase::SetInfo(const std::string& exprInfo, bool pass, bool abort) {
    pass_  = pass;
    abort_ = abort;
    expr_  = (pass_ ? "[passed]: " : "[failed]: ") + exprInfo;
    return *this;
}


void  UnitTestBase::Print() const {
    SetColor(Color_red);
    for (const auto& err : errors_) {
        std::cout << err << std::endl;
    }
    SetColor(Color_normal);
}


void UnitTestBase::FlushError() {
    if (pass_) {
        SetColor(Color_green);
        std::cout << expr_ << std::endl;
        return;
    }

    errors_.push_back(expr_);
    if (abort_) {
        Print();
        ::abort();
    }
}


UnitTestManager& UnitTestManager::Instance() {
    static UnitTestManager mgr;
    return mgr;
}

void UnitTestManager::AddTest(UnitTestBase* test) {
    tests_.push_back(test);
}

void UnitTestManager::Clear() {
    tests_.clear();
}

void UnitTestManager::Run() {
    std::size_t pass = 0;
    std::size_t fail = 0;

    for (auto ut : tests_) {
        ut->Run();
        if (ut->IsFine()) {
            ++ pass;

            SetColor(Color_white);
            std::cout << "ALL PASSED! " << ut->GetName() << std::endl;
        } else {
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
bool f(bool b) {
    return b;
}

TEST_CASE(test1) {
    EXPECT_TRUE(5 != 3) << "fuck, why 5 != 3 failed ? ";
    EXPECT_FALSE(0 != 0);
    EXPECT_FALSE(3 == 2) << "fuck, why 3 == 2 passed? ";
}

TEST_CASE(test2) {
    ASSERT_FALSE(2 != 2) << "fuck, why 2 != 2 passed? ";
    ASSERT_TRUE(f(true)) << "fuck, why failed? ";

    EXPECT_TRUE(9 != 9) << " true raise error!";
    EXPECT_FALSE(0 == 0) << " raise error!";
    ASSERT_FALSE(2 == 2) << " raise abort!";
    ASSERT_FALSE(-1 == 0) << "fuck, why -1 == 0 passed? ";
}
#endif

