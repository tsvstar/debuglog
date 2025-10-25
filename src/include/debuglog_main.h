#pragma once

// Suppress warning: ISO C++11 requires at least one argument for the "..." in a variadic macro
#pragma GCC system_header

/**
  Purpose: Scope/function enter/exit and regular logging
  Author: Taranenko Sergey
  Date: 02-Aug-2017 ... 19-Oct-2024
  License: BSD. See License.txt

  IMPORTANT!! Do not include this file directly or indirectly into PreCompiledHeaders scope,
              otherwise DEBUG_LOGGING / DEBUGLOG_CATEG switching will not work
*/

#include <string>
#include "debuglog_enum.h"

/**
  * DEBUG_LOGGING - determine if SentryLogger macros generate output (SENTRY_*, SAY_*, SAY_DBG)
  * 
  * a) Could be defined as exact value (0 or 1) for local file
  *    #define DEBUG_LOGGING 0 // before #include "debuglog.h"
  * b) Could be defined as a category (DEBUGLOG_CATEG_..) for local file
  *    #define DEBUG_LOGGING DEBUGLOG_CATEG_TEST_ON // before #include "debuglog.h"
  * c) If doesn't defined then DEBUGLOG_CATEG_DEFAULT value is used
  * 
  * Categories are defined in "debuglog_categories.h" 
  */

// Normally is defined in the wrapper include, this is just in case fallback
#ifndef DEBUG_LOGGING
#ifdef DEBUGLOG_CATEG_DEFAULT
#define DEBUG_LOGGING DEBUGLOG_CATEG_DEFAULT
#else
#define DEBUG_LOGGING 1
#endif
#endif

/**
 *  Generic helpers
 **/

#define DEBUGLOG_STRINGIZE_DETAIL(x) #x
#define DEBUGLOG_STRINGIZE(x) DEBUGLOG_STRINGIZE_DETAIL(x)
#define DEBUGLOG_PASTE_IMPL(x,y) x ## y
#define DEBUGLOG_PASTE(x,y) DEBUGLOG_PASTE_IMPL(x,y)

// macro which do nothing as standalone statement but means "false" value in expression
#define SENTRYLOGGER_DO_NOTHING(...) []{return false;}()
// optimized macro which could be used only as standalone statement, but compiled faster
#define SENTRYLOGGER_DO_NOTHING_STANDALONE(...) static_cast<void>(false)
// helpers which expands enter message arguments if parenthesis given
#define SENTRYLOGGER_ENTER_0(...) sentryLoggerEnter{}
#define SENTRYLOGGER_ENTER_1(...) sentryLoggerEnter{TOSTR_ARGS(__VA_ARGS__)}

// Enforce __VA_OPT__ usage if it is possible to avoid non-standard tricks
#if __cplusplus >= 202002L || (defined(__clang__) && __clang_major__ > 8) || (__GNUC__ > 9)
#define __VA_W_COMMA(...) __VA_OPT__(,) __VA_ARGS__
// Macro above is best way to resolve intentionally omited variadic macro
// But before C++20 produce warning - so suppress it
#pragma GCC diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#else
#define __VA_W_COMMA(...) , ## __VA_ARGS__
// Fail early if something wrong (some versions of GCC doesn't process this extension for unknown reason)
static_assert(false __VA_W_COMMA(), "Unable to correctly concatenate empty __VA_ARGS__");
#endif


/**
 *  End-user macro
 **/

// SENTRY_FUNC([{initArgs}]);
// SENTRY_FUNC([{initArgs}])(arg1, arg2, ...);
#define SENTRY_FUNC(...) SENTRYLOGGER_CREATE_1(__PRETTY_FUNCTION__, "" __VA_W_COMMA(__VA_ARGS__))

// SENTRY_CONTEXT("contextName" [,{initArgs}]);
// SENTRY_CONTEXT("contextName" [,{initArgs}])(arg1, arg2, ...);
#define SENTRY_CONTEXT(context,...) SENTRYLOGGER_CREATE_1("|" __FILE__ ":" DEBUGLOG_STRINGIZE(__LINE__), context __VA_W_COMMA(__VA_ARGS__))

// SENTRY_SILENT( std::string_view contextName[, bool appendParentContext = true [, kind = Kind::Default]])
// @note: Defined separately for each section as use specific ctor. The output level could be updated later
// @note: Unlike SENTRY_FUNC/SENTRY_CONTEXT, no args available -- because it never prints enter message.

#define SENTRY_FUNC_COND(allowFlag, ...) DEBUGLOG_PASTE(SENTRYLOGGER_CREATE_, allowFlag)( __PRETTY_FUNCTION__, "" __VA_W_COMMA(__VA_ARGS__))
#define SENTRY_CONTEXT_COND(allowFlag, context, ...) DEBUGLOG_PASTE(SENTRYLOGGER_CREATE_, allowFlag)("|" __FILE__ ":" DEBUGLOG_STRINGIZE(__LINE__), context __VA_W_COMMA(__VA_ARGS__))

#define SAY_DBG_L(level,...)    SENTRYLOGGER_PRINT(level)( __VA_ARGS__)
#define SAY_ARGS_L(level,...)   SENTRYLOGGER_PRINT(level)( TOSTR_ARGS(__VA_ARGS__) )
#define SAY_EXPR_L(level,...)   SENTRYLOGGER_PRINT(level)( TOSTR_EXPR(__VA_ARGS__) )
#define SAY_JOIN_L(level, ...)  SENTRYLOGGER_PRINT(level)( TOSTR_JOIN(__VA_ARGS__) )
#define SAY_FMT_L(level,...)    SENTRYLOGGER_PRINT(level)( TOSTR_FMT(__VA_ARGS__) )

#define SAY_DBG(...)    SENTRYLOGGER_PRINT()( __VA_ARGS__)
#define SAY_ARGS(...)   SENTRYLOGGER_PRINT()( TOSTR_ARGS(__VA_ARGS__) )
#define SAY_EXPR(...)   SENTRYLOGGER_PRINT()( TOSTR_EXPR(__VA_ARGS__) )
#define SAY_JOIN( ...)  SENTRYLOGGER_PRINT()( TOSTR_JOIN(__VA_ARGS__) )
#define SAY_FMT(...)    SENTRYLOGGER_PRINT()( TOSTR_FMT(__VA_ARGS__) )

#define SAY_STACKTRACE(...)  EXECUTE_IF_DEBUGLOG( if (sentryLogger.isAllowedStage(SentryLogger::Stage::Event)) sentryLogger.printStackTrace(__VA_ARGS__))
#define SAY_AND_RETURN(arg, ...) EXECUTE_IF_DEBUGLOG2( \
            {decltype(auto) rv = arg; if (sentryLogger.isAllowed(SentryLogger::Stage::Leave)) sentryLogger.setReturnValueStr( ::tsv::util::tostr::toStr(rv, ::tsv::util::tostr::ENUM_TOSTR_REPR) + TOSTR_JOIN(__VA_ARGS__) ); return rv; }, \
            return arg)  

#define SENTRYLOGGER_DO(action)  EXECUTE_IF_DEBUGLOG2( sentryLogger.action, SENTRYLOGGER_DO_NOTHING )
#define EXECUTE_IF_DEBUGLOG(action)  EXECUTE_IF_DEBUGLOG2( action, SENTRYLOGGER_DO_NOTHING() )
#define IS_LOGGING_OBJ(...) EXECUTE_IF_DEBUGLOG2(::tsv::debuglog::logobjects::isAllowedObj(__VA_ARGS__), false)

// Example of usage: SAY_VIRT_FUNC_CALL( Base, this, func, TOSTR_ARGS() );
#define SAY_VIRT_FUNC_CALL(BaseClass, objPtr, method, ...) EXECUTE_IF_DEBUGLOG2(\
        if (sentryLogger.isAllowed(SentryLogger::Stage::Event)) { \
            const auto* funcPtr = WHICH_VIRTFUNC_WILL_BE_CALLED( BaseClass, objPtr, method ); \
            std::string suf = TOSTR_ARGS(__VA_ARGS__); \
            std::string funcName = ::tsv::debuglog::resolveAddr2Name( funcPtr, ::tsv::debuglog::resolve::settings::btIncludeLine, true); \
            sentryLogger.print( "Call vfunc "+ funcName + suf ); } , \
            SENTRYLOGGER_DO_NOTHING_STANDALONE())

/**
 *  Common code
 */

namespace tsv::debuglog
{
    // Forward declaration
    struct StackTraceArgs;

    // Stub to make safe fallback "SAY_() << something;" and conditional compile-disabled sentries
    struct SentryLoggerStub
    {
        bool isAllowed(...) const { return false; }
        bool isAllowedStage(...) const { return false; }
        bool isAllowedAndSetTempLevel(...) { return false; }
        void printStackTrace() const {}
        void printStackTrace(const StackTraceArgs& , ...) const {}
        void setReturnValueStr(std::string ) {}
        void print([[maybe_unused]]std::string_view content = "") {}
        void setLogLevel(...) {}
        void setFlag(...) {}
        bool checkFlags(...) const {return false;}
    };

    // Truncate func arguments and return value from the __PRETTY_FUNCTION__
    std::string_view extractFuncName(std::string_view prettyFnName);

}

#if DEBUG_LOGGING

namespace tsv::debuglog 
{

/**
 * Core class
 */
class SentryLogger
{
       friend class Settings;

    public:
       // All enums of the class matches to this type
       using EnumType_t = sentry_enum::EnumType_t;
       using Level = sentry_enum::Level;
       using Kind = sentry_enum::Kind;

       enum class Flags : EnumType_t
       {
           Default = 0,            //log everything without special
           SuppressEnter  = 1<<1,
           SuppressLeave  = 1<<2,
           SuppressEvents = 1<<3,
           Timer          = 1<<4, // turn on time tracking (ms resolution)
           Force          = 1<<5, // if true, all kinds and all stages if not out of loglevel
           AppendContextName = 1<<6, // if true, the contextName will be "PreviousContextName--ThisContextName"
           SuppressBorders = SuppressEnter|SuppressLeave
       };

       enum Stage : EnumType_t
       {
          //reuse flags to make trivial check
          Enter = static_cast<EnumType_t>(Flags::SuppressEnter),
          Event = static_cast<EnumType_t>(Flags::SuppressEvents),
          Leave = static_cast<EnumType_t>(Flags::SuppressLeave)
       };

       struct InitArgs
       {
           Kind kind   = Kind::Default;
           Level level = Level::Default;
           Flags flags = Flags::Default;
           bool enabled = true;
           // Track in scope of sentry and subcalls the object "kind:object" (logobjects::isAllowedObj()==true)
           // That is to show in the enter message content of "this" if its output is limited
           void* object = nullptr;
       };

    public:
        // Helper to print enter message
        struct EnterHelper
        {
            EnterHelper(std::string_view content = "");
        };

    public:
        SentryLogger(const char* prettyName = nullptr, std::string_view name = "")
            : SentryLogger(prettyName, name, InitArgs{})
        {}

        SentryLogger(const char* prettyName, std::string_view name, InitArgs args);
        SentryLogger(Level level,
                     std::string_view name,
                     bool appendContextFlag = true,
                     Kind kind = Kind::Default);
        ~SentryLogger();

        SentryLogger(const SentryLogger&) = delete;
        SentryLogger& operator=(const SentryLogger&) = delete;

        void printStackTrace();
        void printStackTrace(StackTraceArgs args, Level level = Level::This);
        void print(std::string_view content = "");

        // Check stage only
        bool isAllowedStage(Stage stage) const;
        bool isAllowed(Stage stage) const
        {
            return isAllowed(stage, mainLogLevel_, kind_);
        }
        bool isAllowed(Stage stage, Level level, Kind kind) const;
        bool isAllowedAndSetTempLevel(Stage stage, Level level);
        bool isAllowedAndSetTempLevel(Stage stage)
        {
            return isAllowedAndSetTempLevel(stage, logLevel_);
        }

        Level getLogLevel() const { return logLevel_; }
        void setLogLevel(Level level);

        void setFlag(Flags flags, bool enable = true);
        auto getFlags() const { return flags_; }
        bool checkFlags(Flags flags) const;

        Kind getKind() const { return kind_; }
        const std::string& getContextName();
        Level transformLogLevel(Level level) const;

        void setReturnValueStr(std::string rv);

        static SentryLogger* getLast();
        static SentryLogger* getRoot();

        // actually output (public for accessing from the LastSentryLogger)
        void write(std::string_view contextName,
                   Stage stage,
                   Level level,
                   std::string_view body,
                   std::string_view suffix);

// TODO: for objlog via getLast() -- do we really need?
        void write(Kind kind,
                   Level level,
                   char nestedSym,
                   std::string_view contextName,
                   std::string_view prefix,
                   std::string_view body,
                   std::string_view suffix);

    private:
        Flags flags_;
        Level logLevel_;     // current log level (could temporary differ from main after setTempLevel)
        Level mainLogLevel_; // the sentry log level (reset to this after each print)
        Kind kind_;
        void* relatedObj_;

        std::string contextName_{};
        // Initialized with string literal for lazy transform to context name.
        // For the SENTRY_FUNC that is a __PRETTY_FUNCTION__
        // For the SENTRY_CONTEXT that is a "|/path/to/file:lineno" (as default for no context)
        const char* prettyName_ = nullptr;

        double startTime_ = 0.0;        // if not 0.0 - starttime since epoch
        static int currentLevel_s;      // current level (depth)
        SentryLogger* prevSentry_;      // uni-direction backward linked list of sentries

        std::string returnValueStr_{};  // empty = no return value, otherwise it contains ". rv = X"

    private:
        // aux class for private ctor
        class RootTag {};

        //Special ctor for root node
        SentryLogger(RootTag);

};
        
/**
 * Wrapper for no-scope SAY_* calls
 * Log using logger on top of stack
*/
class LastSentryLogger
{
    friend class Settings;
    using Level = SentryLogger::Level;

public:
    LastSentryLogger() {}

    void printStackTrace();
    void printStackTrace(StackTraceArgs args, Level level = Level::This);
    void print(std::string_view content = "");
    bool isAllowed(SentryLogger::Stage stage) const
    {
        return SentryLogger::getLast()->isAllowed(stage);
    }
    bool isAllowedStage(SentryLogger::Stage stage) const
    {
        return SentryLogger::getLast()->isAllowedStage(stage);
    }
    bool isAllowedAndSetTempLevel(SentryLogger::Stage stage)
    {
        return SentryLogger::getLast()->isAllowedAndSetTempLevel(stage);
    }
    bool isAllowedAndSetTempLevel(SentryLogger::Stage stage, Level level)
    {
        return SentryLogger::getLast()->isAllowedAndSetTempLevel(stage, level);
    }

// TODO: for objlog.c
    void write(SentryLogger::Kind kind,
               Level level,
               char nestedSym,
               std::string_view contextName,
               std::string_view prefix,
               std::string_view body,
               std::string_view streamBody);

private:
    const std::string& getContextName();
};

struct StackTraceArgs
{
    int depth = -1;         // how many entries are displayed in the stacktrace
    int skip = 0;           // how many first entries should be skipped (to ignore aux frames)
    bool enforce = false;   // if true, ignore Settings::isStackTraceEnabled()
    SentryLogger::Kind kind = SentryLogger::Kind::Parent; // make possible to add secondary kind for stacktrace to separately extend output with it
};


// TODO -- not only yes/no but level of interest

/**
  Registry of objects of interest, which we are interested in.
  That is to make IS_LOGGING_OBJ (trace only related to interested object logs) works.
*/
namespace logobjects
{

class Guard
{
public:
    Guard(const void* objPtr, SentryLogger::Kind kind = SentryLogger::Kind::Default, bool registerObject = true);
    ~Guard();

private:
    SentryLogger::Kind kind_;
    const void* objPtr_;
};

void setAllowAll(bool flag, SentryLogger::Kind kind = SentryLogger::Kind::Default);
void registerObject(SentryLogger::Kind kind, const void* objPtr);
void deregisterObject(SentryLogger::Kind kind, const void* objPtr);

// Return true if objPtr is registered for given kind or its mode is "AllowAll"
// (by default it isn't)
bool isAllowedObj(const void* objPtr, SentryLogger::Kind kind = SentryLogger::Kind::Default);

} // namespace logobjects

} // namespace tsv::debuglog

using tsv::debuglog::SentryLogger;

namespace
{
   ::tsv::debuglog::LastSentryLogger sentryLogger;
}

inline SentryLogger::EnumType_t operator|(const SentryLogger::Flags v1,
                                          const SentryLogger::Flags v2)
{
    return (static_cast<SentryLogger::EnumType_t>(v1)
            | static_cast<SentryLogger::EnumType_t>(v2));
}

/**
 *    Compile-time globally enabled debug logging
 */

#define SENTRY_SILENT(context, ...)  using namespace ::tsv::debuglog; SentryLogger sentryLogger(::tsv::debuglog::SentryLogger::Level::Default, context __VA_W_COMMA(__VA_ARGS__));

#define EXECUTE_IF_DEBUGLOG2( CodeEnabled, CodeDisabled ) CodeEnabled
// Snippet which do nothing but euqiv to SENTRY_* (that is for disabled branch of conditional SENTRY_*_COND)
#define SENTRYLOGGER_CREATE_0(...) using namespace ::tsv::debuglog; [[maybe_unused]] SentryLoggerStub sentryLogger; \
     if (false) [[maybe_unused]] SentryLoggerStub SENTRYLOGGER_ENTER_0
// Arguments: pretty, context[, arg]
#define SENTRYLOGGER_CREATE_1(...) using namespace ::tsv::debuglog; [[maybe_unused]] SentryLogger sentryLogger{__VA_ARGS__}; \
     if (sentryLogger.isAllowed(SentryLogger::Stage::Enter)) SentryLogger::EnterHelper SENTRYLOGGER_ENTER_1
#define SENTRYLOGGER_PRINT(...) if (sentryLogger.isAllowedAndSetTempLevel(SentryLogger::Stage::Event __VA_W_COMMA(__VA_ARGS__))) sentryLogger.print

#else

/**
 *      Compile-time generally disabled debug logging (DEBUGLOGGING = 0)
 */
static_assert(false,"stop2");

#define EXECUTE_IF_DEBUGLOG2(CodeEnabled, CodeDisabled) CodeDisabled

#define SENTRY_SILENT(context, ...) SENTRYLOGGER_DO_NOTHING_STANDALONE()
// Need to care of optional ()() syntax, so use more complex "do nothing" construction
#define SENTRYLOGGER_CREATE_0(...)  if (false) SentryLoggerStub SENTRYLOGGER_ENTER_0
#define SENTRYLOGGER_CREATE_1(...)  if (false) SentryLoggerStub SENTRYLOGGER_ENTER_0
#define SENTRYLOGGER_PRINT(...)     SENTRYLOGGER_DO_NOTHING_STANDALONE

#endif
