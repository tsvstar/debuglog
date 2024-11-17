#include <iostream>
#include <functional>
#include <string>
#include <regex>
#include "tostr_fmt_include.h"

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


// Helper to escape special characters for readability
std::string escapeSpecialChars(const std::string& str)
{
    std::ostringstream escaped;
    for (char c : str)
    {
        escaped << TOSTR_FMT("{}({:#04x})", std::isprint(c)?c:' ',c);
    }
    return escaped.str();
}

// Function to find and display differences
void showDifferences(const std::string& str1, const std::string& str2)
{
    std::size_t len1 = str1.size();
    std::size_t len2 = str2.size();

    // Create a DP table to store edit distances
    std::vector<std::vector<int>> dp(len1 + 1, std::vector<int>(len2 + 1));

    // Fill the table
    for (size_t i = 0; i <= len1; ++i)
    {
        for (size_t j = 0; j <= len2; ++j)
        {
            if (i == 0)
                dp[i][j] = j;  // All insertions
            else if (j == 0)
                dp[i][j] = i;  // All deletions
            else if (str1[i - 1] == str2[j - 1])
                dp[i][j] = dp[i - 1][j - 1];  // No change
            else
            {
                dp[i][j] =
                    1 + std::min({dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1]});  // Min of insert, delete, replace
            }
        }
    }

    // Traceback to find the differences
    std::size_t idx1 = len1, idx2 = len2;
    std::size_t contextStart;
    std::ostringstream diff;

    while (idx1 > 0 || idx2 > 0)
    {
        if (idx1 > 0 && idx2 > 0 && str1[idx1 - 1] == str2[idx2 - 1])
        {
            // No change, move diagonally
            --idx1;
            --idx2;
        }
        else if (idx2 > 0 && (idx1 == 0 || dp[idx1][idx2 - 1] < dp[idx1 - 1][idx2]))
        {
            // Insertion in str2
            diff << "Insert at " << idx1 << ": "
                 << escapeSpecialChars(std::string(1, str2[idx2 - 1])) << "\n";
            contextStart = idx2 < 20 ? 0 : idx2;
            diff << "CONTEXT: " << std::string_view(str2).substr(contextStart, 20)
                 << "!!" << std::string_view(str2).substr(idx2, 20) << "\n";
            --idx2;
        }
        else if (idx1 > 0 && (idx2 == 0 || dp[idx1 - 1][idx2] <= dp[idx1][idx2 - 1]))
        {
            // Deletion in str1
            diff << "Remove at " << idx1 - 1 << ": "
                 << escapeSpecialChars(std::string(1, str1[idx1 - 1])) << "\n";
            contextStart = idx1 <20? 0 : idx1;
            diff << "CONTEXT: " << std::string_view(str2).substr(contextStart, 20)
                 << "!!" << std::string_view(str2).substr(idx2, 20) << "\n";
            --idx1;
        }
        else
        {
            // Substitution (not expected based on your request, but included for completeness)
            diff << "Replace at " << idx1 - 1 << ": "
                 << escapeSpecialChars(std::string(1, str1[idx1 - 1])) << " with "
                 << escapeSpecialChars(std::string(1, str2[idx2 - 1])) << "\n";
            contextStart = idx1 <20? 0 : idx1;
            diff << "CONTEXT: " << std::string_view(str2).substr(contextStart, 20)
                 << "!!" << std::string_view(str2).substr(idx2, 20) << "\n";
            --idx1;
            --idx2;
        }
    }

    // Output the differences
    std::cout << diff.str();
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

/*    
    // Uncomment if not clear what is the real difference
    if (!isOk)
        showDifferences(val, expected);
*/

    return isOk;
}

// Declaration from another test_*.cpp
bool test_tostr();
}

namespace tsv::debuglog::tests::test_sentry1
{
int run();
}

/**************** MAIN() ***************/
int main()
{
    std::cout<< "\n *** TOSTR module ***\n";
    tsv::debuglog::tests::test_tostr();

    std::cout<< "\n *** DEBUGLOG module ***\n";
    tsv::debuglog::tests::test_sentry1::run();

/*
    std::cout<< "\n *** OBJLOG module ***\n";
    test_objlog();

    std::cout<< "\n *** DEBUGWATCH module ***\n";
    test_watcher();
*/
    return 0;
}
