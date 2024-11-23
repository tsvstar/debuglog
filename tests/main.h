#include <string>
#include <vector>

#include "debuglog_main.h"

namespace tsv::debuglog::tests
{

extern bool isOkTotal;
extern const char* testPrefix;
bool test(std::string&& val, const char* expected = nullptr, bool removeAddrFlag = false, bool checkEquality = true );

extern std::string loggedString;

void setupDefault(const std::string& testNamespace = "tsv::debuglog::tests::");
void testLoggerHandlerSentry(SentryLogger::Level level, SentryLogger::Kind kind, std::string_view line);
void testBacktraceHandler(int depth, int /*skip*/, std::vector<std::string>& out);
bool TEST(const char* expected = nullptr, bool removeAddrFlag = false);

}