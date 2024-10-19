#include <iostream>
#include <functional>
#include <string>
#include <regex>

/************** TEST **********/

namespace tsv::debuglog::tests
{

bool test( bool& isOkTotal, const char* prefix, const std::string& val,  std::function<bool(std::string_view)> fn)
{
    std::cout << prefix << val << "\n";
    bool isOk = fn(val);
    isOkTotal = isOkTotal && isOk;
    if ( !isOk )
        std::cout << "TEST FAIL!\n";
    return isOk;

}

bool test( bool& isOkTotal, const char* prefix, const std::string& val, const char* expected = nullptr, bool checkEquality = true )
{
    std::cout << prefix << val << "\n";
    if ( expected == nullptr )
    {
        // Just print
        return true;
    }

    bool isOk = checkEquality ? (val == expected)  : (val != expected);
    isOkTotal = isOkTotal && isOk;
    if ( !isOk )
        std::cout << "TEST FAIL! Should be " << expected << "\n";
    return isOk;
}

std::string removeAddr( std::string input )
{
    std::regex hex_pattern("(0x[0-9a-f]+)", std::regex::icase);
    return std::regex_replace(input, hex_pattern, "0xADDR");
}    

// Declaration from another test_*.cpp
bool test_tostr();
bool test_sentry();
bool test_objlog();
bool test_watcher();

}


/**************** MAIN() ***************/
int main()
{
    using namespace tsv::debuglog::tests;

    std::cout<< "\n *** TOSTR module ***\n";
    test_tostr();
/*
    std::cout<< "\n *** DEBUGLOG module ***\n";
    test_sentry();

    std::cout<< "\n *** OBJLOG module ***\n";
    test_objlog();

    std::cout<< "\n *** DEBUGWATCH module ***\n";
    test_watcher();
*/
    return 0;
}
