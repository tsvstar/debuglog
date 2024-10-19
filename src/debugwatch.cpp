/**
  Purpose:  Watching access to POD and STL members
  Author: Taranenko Sergey (tsvstar@gmail.com)
  Date: 02-Aug-2017 ... 19-Oct-2024
  License: BSD. See License.txt
*/

// Enforce flag to correct declaration and definition
#define DEBUG_LOGGING 1

#include "debugwatch.h"
#include "debuglog_settings.h"
#include "tostr_fmt_include.h"

namespace tsv::debuglog::watch::impl
{

std::string_view globalScope()
{
    static std::string typeName = typeid(Global).name();
    return typeName;
}

void print(int depth, std::string_view comment, std::string_view operation, std::string_view typeName, void* self, std::string_view memberName, std::string_view value)
{
    using namespace ::tsv::debuglog;

    if (Settings::getWatchLogLevel() > Settings::getLogLevel())
        return;

    std::string_view pre{}, post{};
    if (!value.empty())
    {
       pre = " ( ";
       post = " )";
    }

    const char* suffix = "";
    if (!comment.empty() && comment[0])
      suffix = ": ";

    if (typeName==globalScope())
    {
        SentryLogger::getLast()->print( TOSTR_FMT("{}{}{} {}{}{}{}",
                                        comment, suffix, operation,  memberName, pre, value, post));
    }
    else if (!self)
    {
        SentryLogger::getLast()->print( TOSTR_FMT("{}{}{} {}.{}{}{}{}",
                                        comment, suffix, operation, ::tsv::debuglog::demangle(typeName.data()), memberName, pre, value, post));
    }
    {
        SentryLogger::getLast()->print( TOSTR_FMT("{}{}{} {}{{0x{:p}}}.{}{}{}{}",
                                        comment, suffix, operation, ::tsv::debuglog::demangle(typeName.data()), self, memberName, pre, value, post));
    }

    // print calltrace with excluding four extra frames: this, Watch_Getter/Setter, prop_set, and prop::operator
    if (depth)
        SentryLogger::getLast()->printStackTrace({/*.depth=*/depth, /*.skip=*/4}, Settings::getWatchLogLevel());
}

} // namespace tsv::debuglog::watch::impl