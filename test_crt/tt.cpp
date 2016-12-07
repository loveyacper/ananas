#include <future>
#include <iostream>
#include <string>


using std::cerr;
using std::endl; // for test

class Test
{
public:
    Test()
    {
        cerr << __FUNCTION__ << endl;
    }

    ~Test()
    {
        cerr << __FUNCTION__ << endl;
    }
};

bool Ready(std::future<int>& ft)
{
    auto res = ft.wait_for(std::chrono::seconds(0));
    return res == std::future_status::ready;
}

int main()
{
    std::promise<int> pp;
    auto fut = pp.get_future();
    pp.set_value(10);

    Ready(fut);
    if (Ready(fut))
        cerr << fut.get() << endl;
}

