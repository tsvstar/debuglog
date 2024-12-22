#include <iostream>
#include <functional>
#include <string>
#include <regex>
#include <unordered_map>
#include "tostr_fmt_include.h"

#define DEBUG_LOGGING 1
#include "debuglog.h"
#include "main.h"

// In most files this include doesn't needed, but here we set up handler and other settings
#include "debuglog_settings.h"
// In most files this include doesn't needed, but here we set up btEnable
#include "debugresolve.h"


namespace tsv::debuglog
{
// Redefinition of the library function to specify new max value of enum Kind
EnumType_t getNumberOfKinds()
{
    static_assert(static_cast<unsigned>(sentry_enum::Kind::NumberOfKinds) == 25, "Wrong NumberOfKinds");
    return static_cast<sentry_enum::EnumType_t>(sentry_enum::Kind::NumberOfKinds);
}
}

/************** TEST **********/

namespace tsv::debuglog::tests
{

bool isOkTotal = true;
const char* testPrefix = "";
std::string loggedString;

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
    // Comment if not clear what is the real difference
    return;

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

    std::ostringstream diff;
    std::size_t contextStart;
    std::size_t idx1 = len1, idx2 = len2;

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
            contextStart = idx2 < 20 ? 0 : idx2-20;
            diff << "CONTEXT: " << std::string_view(str2).substr(contextStart, idx2<20?idx2:20)
                 << "!!" << std::string_view(str2).substr(idx2, 20) << "\n";
            --idx2;
        }
        else if (idx1 > 0 && (idx2 == 0 || dp[idx1 - 1][idx2] <= dp[idx1][idx2 - 1]))
        {

            if (idx2>0 && (idx1 == 1 || dp[idx1-1][idx2 - 1] < dp[idx1 - 2][idx2]))
            {
                diff << "Replace at " << idx1 - 1 << ": "
                    << escapeSpecialChars(std::string(1, str1[idx1 - 1])) << " with "
                    << escapeSpecialChars(std::string(1, str2[idx2 - 1])) << "\n";
                --idx1;
                --idx2;
                contextStart = idx1 < 20 ? 0 : idx1 - 20;
                diff << "CONTEXT: " << std::string_view(str1).substr(contextStart, idx1<20?idx1:20)
                    << "!!" << std::string_view(str1).substr(idx1, 20) << "\n";
            }
            else
            {
                // Deletion in str1
                diff << "Remove at " << idx1 - 1 << ": "
                    << escapeSpecialChars(std::string(1, str1[idx1 - 1])) << "\n";
                contextStart = idx1 < 20 ? 0 : idx1 - 20;
                diff << "CONTEXT: " << std::string_view(str1).substr(contextStart, idx1<20?idx1:20)
                    << "!!" << std::string_view(str1).substr(idx1, 20) << "\n";
                --idx1;

            }
        }
        else
        {
            diff << "NEVER SHOULD HAPPENS";
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

bool test(std::string&& val, const char* expected/*= nullptr*/, bool removeAddrFlag/*= false*/, bool checkEquality/*= true*/ )
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
   
    if (!isOk)
        showDifferences(val, expected);

    return isOk;
}

void testLoggerHandlerSentry(SentryLogger::Level level, SentryLogger::Kind kind, std::string_view line)
{
    using Level = SentryLogger::Level;
    using Kind = SentryLogger::Kind;
    static std::unordered_map<Level,std::string> levels { {Level::Warning, "Warn"}, {Level::Info,"Info"}, {Level::Error,"Err"}, {Level::Fatal, "Fatal"}};
    static std::unordered_map<Kind,std::string> kinds { {Kind::Default, "Dflt"}, {Kind::TestOn,"TestOn"}, {Kind::TestOff, "TestOff"}, {Kind::Tracked, "Tr"}, {Kind::TestBT,"BT"}};    

//    loggedString += TOSTR_FMT("[{}:{}]{}\n", static_cast<unsigned>(level), static_cast<unsigned>(kind), line);
    loggedString += TOSTR_FMT("[{}:{}]{}\n", levels[level], kinds[kind], line);
}

void testBacktraceHandler(int depth, int /*skip*/, std::vector<std::string>& out)
{
    out.push_back( TOSTR_FMT("stacktrace disabled. depth={}", depth) );
}

bool TEST(const char* expected/*= nullptr*/, bool removeAddrFlag/*= false*/)
{
    auto rv = tsv::debuglog::tests::test(std::move(loggedString), expected, removeAddrFlag, true);
    loggedString.clear();
    return rv;
}

void setupDefault(const std::string& testNamespace)
{
    resolve::settings::btEnable = false;
    resolve::settings::btDisabledOutputCallback = testBacktraceHandler;

    using Op = Settings::Operation;
    Settings::setOutputHandler(testLoggerHandlerSentry);
    SentryLogger::getRoot()->setLogLevel( SentryLogger::Level::Warning );
    Settings::setCutoffNamespaces( {testNamespace} );

    // sample names
    Settings::setKindNames( {{SentryLogger::Kind::TestOn, "[on]"},
                             {SentryLogger::Kind    ::TestOff, "[off]"},
                             {SentryLogger::Kind::Tracked, "[track]"},
                             {SentryLogger::Kind::TestBT, "[BT]"}}
                             );

    Settings::set( { { SentryLogger::Level::Info, Op::SetLogLevel},        // system logging level
                     { SentryLogger::Level::Info, Op::SetDefaultLogLevel}, // this level assigned to sentry if not given

                     // NOTE: Settings::enableStacktrace() and resolve::settings::btEnable are different flags.
                     //    First one determine should SAY_STACKTRACE() do anything or not
                     //    Second determine if stacktrace feature available in the system.
                     { Op::EnableStackTraceTag{}, true},          // stacktrace enabled

                     // Turn off Kind::TestOff, and turn on rest kinds
                     { SentryLogger::Kind::Default, true },
                     { SentryLogger::Kind::TestOn,  true },
                     { SentryLogger::Kind::TestOff, false },
                     // ObjLog default category is turned on
                     { SentryLogger::Kind::Tracked,  true },

                     // Kind could be separated or combined category which contains integer value.
                     // This example is combine totally on/off kind and stacktrace depth
                     // take a look test_sentry to see how it could be compile-time disabled
                     { SentryLogger::Kind::TestBT,  DEBUGLOG_CATEG_TEST_BT }
                   } );
}

}


namespace tsv::debuglog::tests
{
bool test_tostr();
}
namespace tsv::debuglog::tests::test_sentry1
{
void run();
}
namespace tsv::debuglog::tests::test_sentry2
{
void run();
}
namespace tsv::debuglog::tests::objlog
{
void run();
}

/**************** MAIN() ***************/
int main()
{
    std::cout<< "\n *** TOSTR module ***\n";
    tsv::debuglog::tests::test_tostr();

    std::cout<< "\n *** DEBUGLOG module ***\n";
    tsv::debuglog::tests::test_sentry1::run();

    std::cout<< "\n *** DEBUGLOG module - CONDITIONAL COMPILE ***\n";
    tsv::debuglog::tests::test_sentry2::run();


    std::cout<< "\n *** OBJLOG module ***\n";
    tsv::debuglog::tests::objlog::run();
/*
    std::cout<< "\n *** DEBUGWATCH module ***\n";
    test_watcher();
*/
    return 0;
}
