#include <iostream>
#include <functional>
#include <string>
#include <regex>

/************** TEST **********/

namespace tsv::debuglog::tests
{

bool isOkTotal = true;
const char* testPrefix = "";

std::string removeAddr( std::string input )
{
    std::regex hex_pattern("(0x[0-9a-f]+)", std::regex::icase);
    return std::regex_replace(input, hex_pattern, "0xADDR");
}    

bool test(const std::string& val,  std::function<bool(std::string_view)> fn)
{
    std::cout << testPrefix << val << "\n";
    bool isOk = fn(val);
    isOkTotal = isOkTotal && isOk;
    if ( !isOk )
        std::cout << "TEST FAIL!\n";
    return isOk;

}

bool test(std::string&& val, const char* expected = nullptr, bool removeAddrFlag = false, bool checkEquality = true )
{
    if (removeAddrFlag)
       val = removeAddr(val);
    std::cout << testPrefix << val << "\n";
    if ( expected == nullptr )
    {
        // Just print
        return true;
    }

    bool isOk = checkEquality ? (val == expected)  : (val != expected);
    isOkTotal = isOkTotal && isOk;
    if ( !isOk )
        std::cout << "TEST FAIL! Should be \n" << expected << "|\n";
    return isOk;
}

// Declaration from another test_*.cpp
bool test_tostr();
}

namespace tsv::debuglog::test_sentry1
{
int run();
}

/**************** MAIN() ***************/
int main()
{
    std::cout<< "\n *** TOSTR module ***\n";
    tsv::debuglog::tests::test_tostr();

/*    std::cout<< "\n *** DEBUGLOG module ***\n";
    tsv::debuglog::test_sentry1::run();


    std::cout<< "\n *** OBJLOG module ***\n";
    test_objlog();

    std::cout<< "\n *** DEBUGWATCH module ***\n";
    test_watcher();
*/
    return 0;
}
