#pragma once

/**
  Purpose: Scope/function enter/exit and regular logging
  Author: Taranenko Sergey
  Date: 02-Aug-2017 ... 19-Oct-2024
  License: BSD. See License.txt

  IMPORTANT!! Do not include this file directly or indirectly into PreCompiledHeaders scope,
              otherwise DEBUG_LOGGING / DEBUGLOG_CATEG switching will not work
*/

#include <string>

// Normally is defined in the wrapper include, this is just in case fallback
#ifndef DEBUG_LOGGING
#ifdef DEBUGLOG_CATEG_DEFAULT
#define DEBUG_LOGGING DEBUGLOG_CATEG_DEFAULT
#else
#define DEBUG_LOGGING 1
#endif
#endif

// If 0 - no operator<< interface available, but performance of operations is better
// If 1 - allowed operator<< for SAY/SENTRY but that impact on speed even if such interface is not used
#ifndef DEBUGLOG_ALLOW_STREAM_INTERFACE
#define DEBUGLOG_ALLOW_STREAM_INTERFACE 1
#endif

#if DEBUGLOG_ALLOW_STREAM_INTERFACE
#include <sstream>
#endif

//Helper macro to select the correct macro based on the number of arguments
#define _DEBUGLOG_GET_MACRO(_1, _2, NAME, ...) NAME


// End-user macro
#define _SENTRY_FUNC_NOARGS()      SENTRYLOGGER_CREATE_1( ::tsv::debuglog::extractFuncName(__PRETTY_FUNCTION__), {} )
#define _SENTRY_FUNC_ARGS(arg,...) SENTRYLOGGER_CREATE_1( ::tsv::debuglog::extractFuncName(__PRETTY_FUNCTION__), arg, TOSTR_ARGS(__VAR_ARGS__) )
#define SENTRY_FUNC(...)           _DEBUGLOG_GET_MACRO(__VA_ARGS__, _SENTRY_FUNC_ARGS, _SENTRY_FUNC_NOARGS)(__VA_ARGS__)

#define _SENTRY_CONTEXT_NOARGS(context)        SENTRYLOGGER_CREATE_1( context, {} )
#define _SENTRY_CONTEXT_ARGS(context,arg,...)  SENTRYLOGGER_CREATE_1( context, arg, TOSTR_ARGS(__VAR_ARGS__) )
#define SENTRY_CONTEXT(context, ...)           _DEBUGLOG_GET_MACRO(__VA_ARGS__, _SENTRY_CONTEXT_ARGS, _SENTRY_CONTEXT_NOARGS)(context, __VA_ARGS__)

#define SENTRY_SILENT()      SENTRYLOGGER_CREATE_1( ::tsv::debuglog::extractFuncName(__PRETTY_FUNCTION__), {SentryLogger::Level::Default, SentryLogger::Kind::Default, SentryLogger::Flags::SuppressBorders} )

// SENTRY_FUNC_W_ARGS({/*.kind=SentryLogger::Kind::Default, .enabled=true*/}, a, b, c}

#define SENTRY_FUNC_COND(allowFlag, arg, ...) SENTRYLOGGER_CREATE_ ## allowFlag( ::tsv::debuglog::extractFuncName(__PRETTY_FUNCTION__), arg, TOSTR_ARGS(__VAR_ARGS__) )
#define SENTRY_CONTEXT_COND(allowFlag, arg, ...) SENTRYLOGGER_CREATE_ ## allowFlag( context, arg, TOSTR_ARGS(__VAR_ARGS__) )

#define SAY_DBG         SENTRYLOGGER_PRINT
#define SAY_STACKTRACE(...)  EXECUTE_IF_DEBUGLOG( if (sentryLogger.isAllowed(SentryLogger::Stage::Event, SentryLogger::Level::Fatal)) [[clang::noinline]] [[gnu::noinline]] sentryLogger.printStackTrace(__VAR_ARGS__))
#define SAY_ARGS(...)   SENTRYLOGGER_PRINT( TOSTR_ARGS(__VA_ARGS__) )
#define SAY_EXPR(...)   SENTRYLOGGER_PRINT( TOSTR_EXPR(__VA_ARGS__) )
#define SAY_JOIN(...)   SENTRYLOGGER_PRINT( TOSTR_JOIN(__VA_ARGS__) )
#define SAY_FMT(...)    SENTRYLOGGER_PRINT( TOSTR_FMT(__VA_ARGS__) )
#define SAY_AND_RETURN(arg,...) EXECUTE_IF_DEBUGLOG2( \
            {decltype(auto) rv = arg; if (sentryLogger.isAllowed(SentryLogger::Stage::Leave)) sentryLogger.setReturnValueStr( ::tsv::util::tostr::toStr(rv, ::tsv::util::tostr::ENUM_TOSTR_REPR) + TOSTR_JOIN(__VAR_ARGS__) ); return rv; }, \
            return arg)  

//TODO: (static_cast<void>(false), false)  -- left side of operator make this work as standalone statement, while comma operator and right side makes this valid expression
#define SENTRYLOGGER_DO_NOTHING(...) static_cast<void>(false)
#define SENTRYLOGGER_DO(action)  EXECUTE_IF_DEBUGLOG2( sentryLogger.action, SENTRYLOGGER_DO_NOTHING )
#define EXECUTE_IF_DEBUGLOG(action)  EXECUTE_IF_DEBUGLOG2( action, SENTRYLOGGER_DO_NOTHING() )
#define IS_LOGGING_OBJ(...) EXECUTE_IF_DEBUGLOG2(::tsv::debuglog::logobject::isAllowedObj(__VA_ARGS__), false)

// Example of usage: SAY_VIRT_FUNC_CALL( Base, this, func, TOSTR_ARGS() );
#define SAY_VIRT_FUNC_CALL(BaseClass, objPtr, method, ...) EXECUTE_IF_DEBUGLOG2(\
        if (sentryLogger.isAllowed(SentryLogger::Stage::Event)) { \
            const auto* funcPtr = WHICH_VIRTFUNC_WILL_BE_CALLED( BaseClass, objPtr, method ); \
            std::string suf = TOSTR_ARGS(__VA_ARGS__); \
            std::string funcName = ::tsv::debuglog::resolveAddr2Name( funcPtr, ::tsv::debuglog::resolve::settings::btIncludeLine, true); \
            sentryLogger.print( "Call vfunc "+ funcName + suf ); } , \
            SENTRYLOGGER_DO_NOTHING())
        

namespace tsv::debuglog
{
    // stub to make safe fallback "SAY_() << something;" and conditional compile-disabled sentries
    struct SentryLoggerStub
    {
        bool isAllowed(...) const { return false; }
        void printStackTrace(...) const {}
        void setReturnValueStr([[maybe_unused]]std::string rv) {}
        void print([[maybe_unused]]std::string_view content = "") {}
        SentryLoggerStub printStreamy([[maybe_unused]]std::string_view content = "") { return *this;}
        template<typename T> SentryLoggerStub& operator<<([[maybe_unused]]const T& val)
        {
            return *this;
        }
    };
}

/**
 *    Compile-time globally enabled debug logging
 */

#if DEBUG_LOGGING

#define EXECUTE_IF_DEBUGLOG2( CodeEnabled, CodeDisabled ) CodeEnabled

#define SENTRYLOGGER_CREATE_0(func, pretty, arg, ...) using namespace ::tsv::debuglog; SentryLoggerStub sentryLogger(); if (sentryLogger.isAllowed(false)) sentryLogger.printStreamy()

#if DEBUGLOG_ALLOW_STREAM_INTERFACE
#define SENTRYLOGGER_CREATE_1(func, pretty, arg, ...) using namespace ::tsv::debuglog; SentryLogger sentryLogger(func, pretty, arg); if (sentryLogger.isAllowed(SentryLogger::Stage::Enter)) sentryLogger.printEnterStreamy(__VA_ARGS__)
#define SENTRYLOGGER_PRINT(...) if (sentryLogger.isAllowed(SentryLogger::Stage::Event)) sentryLogger.printStreamy(__VA_ARGS__)
#else
#define SENTRYLOGGER_CREATE_1(func, pretty, arg, ...) using namespace ::tsv::debuglog; SentryLogger sentryLogger(func, pretty, arg); if (sentryLogger.isAllowed(SentryLogger::Stage::Enter)) sentryLogger.printEnter(__VA_ARGS__)
#define SENTRYLOGGER_PRINT(...) if (sentryLogger.isAllowed(SentryLogger::Stage::Event)) sentryLogger.print(__VA_ARGS__)
#endif

// Whole-file scope. Used if no function or local scope defined and refers to the last logger in the stack.
// The context name of such unscoped SAY_* could be extra specified by defining FILENAME_FOR_DEBUGLOG before #include "debuglog.h".
// Sample usage is "#define FILENAME_FOR_DEBUGLOG __FILE__"
#ifndef FILENAME_FOR_DEBUGLOG
#define FILENAME_FOR_DEBUGLOG ""
#endif


namespace tsv::debuglog 
{

struct StackTraceArgs;

/**
 * Core class
*/

class SentryLogger
{
       friend class Settings;

    public:
       // All enums of the class matches to this type
       using EnumType_t = unsigned;

       enum class Level : EnumType_t
       {
          Fatal,
          Error,
          Warning,
          Important,
          Info,
          Debug,
          Trace,
          // Special cases. Important to keep this block Off-Default-Parent as is at the end
          Off,      // Turn off logger  
          Default,  // Special kind - replaced to Settings::getDefaultSentryLoggerLevel()
          This,     // Special kind - means level of the logger
          Parent    // Special kind - means level of upper level logger
       };

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

       enum class Kind : EnumType_t
       {
          Default,
          Tracked,        // built-in category for objlog module
          KnownPtr,       // built-in category for register and output that
          Test1,          // that is for unittest purpose

          //..your own categories here

          // Special cases. Important to keep this block NumberOfKinds-Off-Parent as is at the end
          NumberOfKinds, 
          Off = NumberOfKinds,
          Parent          // Special kind - means kind of sentry
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
           Level level = Level::Default;
           Kind kind   = Kind::Default;
           Flags flags = Flags::Default;
           void* object = nullptr;	// track in scope of sentry and subcalls the object "kind:object" (logobjects::isAllowedObj()==true)
					//todo: -- not sure that place it here is usefull. maybe that should be synonym to .enable = logobjects::isAllowedObj() ??
           bool enabled = true;
           //std::string contextName{}; //todo: -- combine SENTRY_FUNC/SENTRY_CONTEXT. why??
       };

#if DEBUGLOG_ALLOW_STREAM_INTERFACE
    public:

        // Aux class to provide ostream operator<< iface
        // Also do logger stage switching and actual output using write()
        class StreamHelper
        {
        public:
            StreamHelper(SentryLogger& log,
                         const std::string_view contextName,
                         const std::string_view prefix,
                         SentryLogger::Stage stage,
                         SentryLogger::Level level);
            ~StreamHelper();

            template <typename T>
            StreamHelper& operator<<(const T& val)
            {
                if (!stream_)
                    stream_ = new std::ostringstream();
                *stream_ << val;
                return *this;
            }

            StreamHelper& operator<<(const Level& level);

            // Important: This change the level for the line, but still doesn't allow print
            // if SentryLogger overall level was lower than getLogLevel()
            /*template<>
            StreamHelper& operator<< <Level>(const Level& level);*/

        private:
            std::ostringstream* stream_ = nullptr; //todo - try void*
            SentryLogger& logger_;
            // operator<< syntax shouldn't be used in other manner than temporary var, so context name exists longer than this object,
            // so we can use string_view
            const std::string_view contextName_;
            const std::string_view body_;
            Stage stage_;
            Level level_;
        };
#endif
    public:
        SentryLogger(InitArgs args,
                     std::string_view name = "",
                     const char* prettyName = nullptr);
        ~SentryLogger();

        SentryLogger(const SentryLogger&) = delete;
        SentryLogger& operator=(const SentryLogger&) = delete;

        void printStackTrace(StackTraceArgs args, Level level = Level::This);
#if DEBUGLOG_ALLOW_STREAM_INTERFACE
        SentryLogger::StreamHelper printEnterStreamy(std::string_view content = "");
        SentryLogger::StreamHelper printStreamy(std::string_view content = "");

        // Stream interface for direct call on the object
        template <typename T>
        SentryLogger::StreamHelper operator<<(const T& val)
        {
            StreamHelper helper(*this, getContextName(), "", Stage::Event, logLevel_);
            helper << val;
            return helper;
        }
#endif
        void printEnter(std::string_view content = "");
        void print(std::string_view content = "");
        bool isAllowed(Stage stage) const;
        bool isAllowed(Stage stage, Level level, Kind kind) const;

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
                   std::string_view streamBody);
        void write(Kind kind,
                   Level level,
                   char nestedSym,
                   std::string_view contextName,
                   std::string_view prefix,
                   std::string_view body,
                   std::string_view streamBody);

    private:
        Flags flags_;
        Level logLevel_;
        Kind kind_;
        void* relatedObj_;

        std::string contextName_{};
        // special case - if prettyFuncName_ is not nullptr, then we initialize
        // SentryLogger with __PRETTY_FUNCTION__ and wait for lazy build contextName_
        const char* prettyFuncName_ = nullptr;
        std::string_view funcName_{};

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
 * Log using logger on top of stack. If extraContext given - it is added to contextName
*/
class LastSentryLogger
{
    friend class Settings;

public:
    LastSentryLogger(std::string_view extraContext = "");

    void printStackTrace(StackTraceArgs args,
                         SentryLogger::Level level = SentryLogger::Level::This);
#if DEBUGLOG_ALLOW_STREAM_INTERFACE
    SentryLogger::StreamHelper printEnterStreamy(std::string_view content = "");
    SentryLogger::StreamHelper printStreamy(std::string_view content = "");
#endif
    void printEnter(std::string_view content = "");
    void print(std::string_view content = "");
    bool isAllowed(SentryLogger::Stage stage) const
    {
        return SentryLogger::getLast()->isAllowed(stage);
    }

    void write(SentryLogger::Kind kind,
               SentryLogger::Level level,
               char nestedSym,
               std::string_view contextName,
               std::string_view prefix,
               std::string_view body,
               std::string_view streamBody);

private:
    const std::string& getContextName();

    std::string extraContextName_;
    std::string fullContextName_;
};

struct StackTraceArgs
{
    int depth = -1;         // how many entries are displayed in the stacktrace
    int skip = 0;           // how many first entries should be skipped (to ignore aux frames)
    bool enforce = false;   // if true, ignore Settings::isStackTraceEnabled()
    SentryLogger::Kind kind = SentryLogger::Kind::Parent; // make possible to add secondary kind for stacktrace to separately extend output with it
    //std::string_view prefix{};  //
};


inline bool SentryLogger::isAllowed(SentryLogger::Stage stage) const
{
    return isAllowed(stage, logLevel_, kind_);
}


/**
  Registry of objects of interest, which we are interested in.
  That is to make IS_LOGGING_OBJ (trace only related to interested object logs) works.
*/
namespace logobjects
{

class Guard
{
public:
    Guard(SentryLogger::Kind kind, const void* objPtr, bool registerObject = true);
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
bool isAllowedObj(void* objPtr, SentryLogger::Kind kind = SentryLogger::Kind::Default);

} // namespace logobjects

constexpr auto extractFuncName(std::string_view pretty_function)
{
    // Find the last space before the opening parenthesis
    auto paren_pos = pretty_function.rfind('(');
    if (paren_pos == std::string_view::npos)
    {
        return std::string_view{};
    }

    auto last_space = pretty_function.rfind(' ', paren_pos);
    if (last_space == std::string_view::npos)
    {
        return std::string_view{};
    }

    // Extract everything after the last space up to the opening parenthesis
    return pretty_function.substr(last_space + 1, paren_pos - last_space - 1);
}

} // namespace tsv::debuglog

using tsv::debuglog::SentryLogger;

namespace
{
   ::tsv::debuglog::LastSentryLogger sentryLoger(FILENAME_FOR_DEBUGLOG);
}

inline SentryLogger::EnumType_t operator|(const SentryLogger::Flags v1,
                                          const SentryLogger::EnumType_t v2)
{
    return (static_cast<SentryLogger::EnumType_t>(v1)
            | static_cast<SentryLogger::EnumType_t>(v2));
}

#else

/**
 *      Compile-time generally disabled debug logging (DEBUGLOGGING = 0)
 */

#define EXECUTE_IF_DEBUGLOG2( CodeEnabled, CodeDisabled ) CodeDisabled

#if DEBUGLOG_ALLOW_STREAM_INTERFACE
#define SENTRYLOGGER_CREATE_0(func, pretty, arg, ...) using namespace ::tsv::debuglog; SentryLoggerStub sentryLogger(); if (sentryLogger.isAllowed(false)) sentryLogger.printStreamy()
#define SENTRYLOGGER_CREATE_1(func, pretty, arg, ...) using namespace ::tsv::debuglog; SentryLoggerStub sentryLogger(); if (sentryLogger.isAllowed(false)) sentryLogger.printStreamy()
#define SENTRYLOGGER_PRINT(...)      if (sentryLogger.isAllowed(false)) ::tsv::debuglog::SentryLoggerStub()


#else
#define SENTRYLOGGER_CREATE_0(func, pretty, arg, ...) SENTRYLOGGER_DO_NOTHING()
#define SENTRYLOGGER_CREATE_1(func, pretty, arg, ...) SENTRYLOGGER_DO_NOTHING()
#define SENTRYLOGGER_PRINT(...)      SENTRYLOGGER_DO_NOTHING()
#endif

#endif
