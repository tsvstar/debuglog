#pragma once

/**
  Purpose: Extends scope sentry/logger with settings
  Author: Taranenko Sergey
  Date: 02-Aug-2017 ... 19-Oct-2024
  License: BSD. See License.txt

  NOTE. Just like "debuglog.h" it shouldn't be directly or indirectly included into PreCompiledHeaders scope
*/

#include "debuglog.h"

#if DEBUG_LOGGING

#include <vector>

namespace tsv::debuglog 
{

// The function in user-code which overrides default one and determine real size of SentryLogger::Kind max element
extern sentry_enum::EnumType_t getNumberOfKinds() __attribute__((weak));

class Settings
{
    using Kind = SentryLogger::Kind;
    using Level = SentryLogger::Level;
    using EnumType_t = SentryLogger::EnumType_t;

public:
    friend class SentryLogger;

    typedef void (*OutputHandler)(SentryLogger::Level level, SentryLogger::Kind kind, std::string_view str);

    struct KindNamePair
    {
        Kind kind_;
        std::string name_;
    };

    // helper class for set
    struct Operation
    {
        enum Type
        {
            SetKind,
            SetLogLevel,
            SetDefaultLogLevel,
            SetWatchLogLevel,
            SetEnableStacktraceFlag
        };
        struct EnableStackTraceTag {};

        Type type_;
        SentryLogger::EnumType_t enumValue_;
        int value_;

        Operation(SentryLogger::Kind kind, int enableFlag);
        Operation(SentryLogger::Level level, Type type);
        Operation(EnableStackTraceTag tag, bool enableFlag);

        Operation();    // to make possible std::vector<Operation>
        Operation(const Operation&) = default;
        Operation& operator=(const Operation&) = default;
    };

    // RAII object to temporary change settings
    struct TemporarySettings
    {
        // Apply given sequence of settings. Rollback all (flag is false) or only that
        // which was changed initially(flag is true) when going out of scope.
        TemporarySettings(std::vector<Operation> ops, bool rollbackOnlyChanged = true);
        ~TemporarySettings();
    private:
        std::vector<Operation> operations_;
    };

    // Batch settings applying
    static void set( std::vector<Operation> ops );

    // todo: not flag but level + system log level
    static bool isKindAllowed(SentryLogger::Kind kind);
    static int getLoggerKindState(SentryLogger::Kind kind);
    static SentryLogger::Level getLoggerKindStateAsLevel(SentryLogger::Kind kind);
    static void setLoggerKindState(SentryLogger::Kind kind, bool enableFlag = true);
    static void setLoggerKindState(SentryLogger::Kind kind, int value);
    static void setLoggerKindState(SentryLogger::Kind kind, SentryLogger::Level value);

    static auto getLogLevel() { return logLevel_s; }
    static auto getWatchLogLevel() { return watchLogLevel_s; }
    static bool isStacktraceEnabled() { return stackTraceEnabled_s; }

    static void setLogLevel(SentryLogger::Level level);
    static void setWatchLogLevel(SentryLogger::Level level);
    static void enableStacktrace(bool flag = true);

    static bool getPrintContextFlag() { return printContextFlag_s; }
    static void setPrintContextFlag(bool flag);

    static SentryLogger::Level getDefaultLevel();
    static void setDefaultLogLevel(SentryLogger::Level level);

    static void setOutputHandler(OutputHandler handler);
    static void setLoggerPrefix(std::string_view prefix);

    static std::vector<std::string>& cutoffNamespaces();
    static void setCutoffNamespaces(std::vector<std::string> arr);

    // Initialize kind->name map used for output if printKindFlag_s==true
    static void setKindNames(const std::vector<KindNamePair>& names);

    // for unittest
    static void setNestedLevelFlag(bool flag);

private:
    // if true, then align by/display depth of sentries nesting level
    static bool isNestedLevelMode_s;
    // if true, print name of context for each output line
    static bool printContextFlag_s;
    // @todo -- what I expect to do here?
    static bool printContextName_s;
    // if true, print name of kind
    static bool printKindFlag_s;
    // string which added into beginning of each logger output line
    static std::string loggerPrefix_s;
    // lines with less important level than this are ignored
    static SentryLogger::Level logLevel_s;
    // real level of SentryLogger::Level::Default value
    static SentryLogger::Level defaultSentryLoggerLevel_s;
    //
    static SentryLogger::Level watchLogLevel_s;
    // only if true, printStacktrace() do its job
    static bool stackTraceEnabled_s;
    // handler which actually do output of prepared by logger lines
    static OutputHandler outputHandler_s;

    // Reverse ordered vector of namespace prefixes which are removed
    // from auto-generated context names. The only first found is cuted.
    static std::vector<std::string> cutoffNamespaces_s;

    // instance of the kind flags array
    // (function to controllable lifetime and init time)
    static int* getKindsStateArray();
};

} // namespace tsv::debuglog
#endif
