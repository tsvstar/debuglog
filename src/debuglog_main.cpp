/**
  Purpose: Scope/function enter/exit and regular logging
  Author: Taranenko Sergey
  Date: 02-Aug-2017 ... 19-Oct-2024
  License: BSD. See License.txt
*/

// Always enforce flags with 1 here because we need full class declarations here
#define DEBUG_LOGGING 1
#define DEBUGLOG_ALLOW_STREAM_INTERFACE 1

#include "debuglog_settings.h"
#include "debugresolve.h"

#include "tostr_fmt_include.h"
#include <chrono>
#include <unordered_set>

namespace
{
void defaultLoggerHandler([[maybe_unused]]SentryLogger::Level level, [[maybe_unused]]std::string_view line)
{
    //if (level <= Settings::getLogLevel()) fmt::print("[dbg]{}\n", line);
}
}

namespace tsv::debuglog
{

/**
    STATIC VARIABLES + SETTINGS
*/
bool Settings::stackTraceEnabled_s = true;
bool Settings::isNestedLevelMode_s = true;   // if true, then show graphically hierarchy
bool Settings::printContextName_s  = true;  
bool Settings::printKindFlag_s = true;
SentryLogger::Level Settings::logLevel_s = SentryLogger::Level::Info;
SentryLogger::Level Settings::defaultSentryLoggerLevel_s = SentryLogger::Level::Info;
SentryLogger::Level Settings::watchLogLevel_s  = SentryLogger::Level::Default;
Settings::OutputHandler Settings::outputHandler_s = defaultLoggerHandler;

int SentryLogger::currentLevel_s  = 0 ;

namespace
{

//TODO: singletone to make sure it already initialized

// Prefixes which are printed for each kind
std::array<std::string_view, static_cast<SentryLogger::EnumType_t>(SentryLogger::Kind::NumberOfKinds)> kindNamesStr =
{
    ""  // Default - no special mark
        //,"[Sample]"   // Sample correspondance kind to its output signature
};

}  // anonymous namespace


namespace
{
bool isStageAllowed(SentryLogger::Flags flags, SentryLogger::Stage stage)
{
    // trick: stages match to corresponding Suppress flags
    return !(static_cast<SentryLogger::EnumType_t>(flags)
             & static_cast<SentryLogger::EnumType_t>(stage));
}

}

/**
    Settings
*/
int* Settings::getKindsStateArray()
{
    // Default everything is turned off
    static int kindsStateArray[static_cast<SentryLogger::EnumType_t>(SentryLogger::Kind::NumberOfKinds)
                                + 1]{};
    return kindsStateArray;
}

int Settings::getLoggerKindState(SentryLogger::Kind kind)
{
    if (kind < SentryLogger::Kind::NumberOfKinds)
        return getKindsStateArray()[static_cast<SentryLogger::EnumType_t>(kind)];
    return 0;
}

SentryLogger::Level Settings::getLoggerKindStateAsLevel(SentryLogger::Kind kind)
{
    using EnumType_t = SentryLogger::EnumType_t;
    EnumType_t rv = static_cast<EnumType_t>(SentryLogger::Level::Off);
    if (kind < SentryLogger::Kind::NumberOfKinds)
        rv = static_cast<EnumType_t>(getKindsStateArray()[static_cast<EnumType_t>(kind)]);
    if (rv > static_cast<EnumType_t>(SentryLogger::Level::Off))
        rv = static_cast<EnumType_t>(SentryLogger::Level::Off);
    return static_cast<SentryLogger::Level>(rv);
}

void Settings::setLoggerKindState(SentryLogger::Kind kind, bool enableFlag /*=true*/)
{
    if (kind < SentryLogger::Kind::NumberOfKinds)
        getKindsStateArray()[static_cast<SentryLogger::EnumType_t>(kind)] = enableFlag;
}

void Settings::setLoggerKindState(SentryLogger::Kind kind, int value)
{
    if (kind < SentryLogger::Kind::NumberOfKinds)
        getKindsStateArray()[static_cast<SentryLogger::EnumType_t>(kind)] = value;
}
void Settings::setLoggerKindState(SentryLogger::Kind kind, SentryLogger::Level value)
{
    if (kind < SentryLogger::Kind::NumberOfKinds)
        getKindsStateArray()[static_cast<SentryLogger::EnumType_t>(kind)] = static_cast<int>(value);
}

void Settings::setLogLevel(SentryLogger::Level level)
{
    if (level <= SentryLogger::Level::Off)
        logLevel_s = level;
}

void Settings::setWatchLogLevel(SentryLogger::Level level)
{
    if (level <= SentryLogger::Level::Off)
        watchLogLevel_s = level;
}


void Settings::enableStacktrace(bool flag /*= true*/)
{
    stackTraceEnabled_s = flag;
}

void Settings::setPrintContextFlag(bool flag)
{
    printContextFlag_s = flag;
}

void Settings::setDefaultLogLevel(SentryLogger::Level level)
{
    if (level <= SentryLogger::Level::Off)
        defaultSentryLoggerLevel_s = level;
}

void Settings::setOutputHandler(OutputHandler handler)
{
    outputHandler_s = handler;
}

void Settings::setLoggerPrefix(std::string_view prefix)
{
    loggerPrefix_s = prefix;
}

Settings::Operation::Operation()
{
    // initialize with values which do nothing
    type_ = Type::SetLogLevel;
    enumValue_ = static_cast<SentryLogger::EnumType_t>(SentryLogger::Level::Parent);
}

Settings::Operation::Operation(SentryLogger::Kind kind, bool enableFlag)
{
    type_ = Type::SetKind;
    enumValue_ = static_cast<SentryLogger::EnumType_t>(kind);
    value_ = enableFlag;
}

Settings::Operation::Operation(SentryLogger::Level level, Type type/*= = Type::SetLogLevel*/)
{
    type_ = (type==Type::SetKind) ? Type::SetLogLevel : type;
    enumValue_ = static_cast<SentryLogger::EnumType_t>(level);
}

Settings::Operation::Operation(Settings::Operation::EnableStackTraceTag /*kind*/, bool enableFlag)
{
    type_ = Type::SetEnableStacktraceFlag;
    value_ = enableFlag;
}

void Settings::set( std::vector<Operation> operations )
{
    for (auto& op : operations)
    {
        if (op.type_==Settings::Operation::Type::SetKind)
            setLoggerKindState(static_cast<SentryLogger::Kind>(op.enumValue_), op.value_);
        else if (op.type_==Settings::Operation::Type::SetLogLevel)
            setLogLevel(static_cast<SentryLogger::Level>(op.enumValue_));
        else if (op.type_==Settings::Operation::Type::SetDefaultLogLevel)
            setDefaultLogLevel(static_cast<SentryLogger::Level>(op.enumValue_));
        else if (op.type_==Settings::Operation::Type::SetWatchLogLevel)
            setWatchLogLevel( static_cast<SentryLogger::Level>(op.enumValue_));
        else
            enableStacktrace(op.value_);
    }
}

Settings::TemporarySettings::TemporarySettings(std::vector<Settings::Operation> operations,
     bool rollbackOnlyChanged/* = true*/)
         : operations_( std::move(operations) )
{
    // Only different values are remembered

    size_t idx = 0;
    for (auto& op : operations_ )
    {
        if (op.type_ == Operation::SetKind)
        {
            auto kind = static_cast<SentryLogger::Kind>(op.enumValue_);
            bool flag = op.value_;
            if (rollbackOnlyChanged && flag == Settings::isKindAllowed(kind))
                continue;
            
            Settings::setLoggerKindState(kind, flag);
            operations_[idx++] = Operation(kind, !flag);
        }
        else if (op.type_ == Operation::SetEnableStacktraceFlag)
        {
            bool flag = op.value_;
            if (rollbackOnlyChanged && flag == Settings::isStacktraceEnabled())
                continue;
            
            Settings::enableStacktrace(flag);
            operations_[idx++] = Operation(Operation::EnableStackTraceTag{}, !flag);
        }
        else
        {
            auto curLevel = Settings::getLogLevel();
            if (op.type_==Operation::SetWatchLogLevel) 
                curLevel = Settings::getWatchLogLevel();
            else if (op.type_==Operation::SetDefaultLogLevel) 
                curLevel = Settings::defaultSentryLoggerLevel_s;

            auto newLevel = static_cast<SentryLogger::Level>(op.enumValue_);
            if (rollbackOnlyChanged && (curLevel == newLevel || newLevel > SentryLogger::Level::Off))
                continue;
            operations_[idx++] = Operation(curLevel, op.type_);

            if (op.type_==Operation::SetWatchLogLevel) 
                Settings::setLogLevel(newLevel);
            else if (op.type_==Operation::SetDefaultLogLevel) 
                Settings::setDefaultLogLevel(newLevel);
            else
                Settings::setWatchLogLevel(newLevel);
        }
    }
    operations_.resize(idx);
}


Settings::TemporarySettings::~TemporarySettings()
{
    Settings::set( operations_ );
}


/**
    Aux functions
*/
namespace
{
double getCurTimestampAsDouble()
{
    using namespace std::chrono;
    auto now = system_clock::now();
    return duration_cast<duration<double>>(now.time_since_epoch()).count();
}

/**
   Get full function name with namespace/class (from __PRETTY_FUNCTION__ and __func__)
   prettyName is something like "rvtype path::to::function(arg1type ...."
   funcName is just "function"
*/
std::string_view findFuncName(std::string_view prettyName, std::string_view funcName)
{
    std::string_view::size_type pos = 0;
    std::string_view::size_type nextPos = 0;

    // Try to find "::name(" or " name(" pattern
    for (;; pos++)
    {
        pos = prettyName.find(funcName, pos);
        // No pattern found - use short name
        if (pos == std::string_view::npos)
            return funcName;
        if (pos < 1)
            continue;
        if (prettyName[pos - 1] != ':' && prettyName[pos - 1] != ' ')
            continue;
        nextPos = pos + funcName.size();
        if (nextPos < prettyName.size() && prettyName[nextPos] == '(')
            break;
    }

    // pattern found - find begin of whole name and its size
    if (prettyName[pos] == ':')
        pos = prettyName.rfind(" ", pos);
    if (pos == std::string_view::npos)
        return prettyName.substr(0, nextPos);
    else
        return prettyName.substr(pos + 1, nextPos - pos - 1);
}

// Aux function to controllable lifetime and initialize of "last_s"
SentryLogger** getLastSentryPtr()
{
    static SentryLogger* last_s = SentryLogger::getRoot();
    return &last_s;
}

} // anonymous namespace

/**
    Main section
*/


/**
    Private ctor for root
*/
SentryLogger::SentryLogger(RootTag)
{
    contextName_ = "core";
    prettyFuncName_ = nullptr;
    flags_ = SentryLogger::Flags::SuppressLeave;
    logLevel_ = Settings::defaultSentryLoggerLevel_s;
    kind_ = SentryLogger::Kind::Default;
    prevSentry_ = nullptr;
}

SentryLogger::SentryLogger(InitArgs args,
                           std::string_view name /*=""*/,
                           const char* prettyName /*=nullptr*/)
    : flags_(args.flags)
    , logLevel_(args.enabled ? args.level : Level::Off)
    , kind_(args.kind > Kind::NumberOfKinds ? Kind::Off : args.kind)
    , relatedObj_(args.object)
    , contextName_(name)
    , prettyFuncName_(prettyName)
{
    if (checkFlags(Flags::Timer))
        startTime_ = getCurTimestampAsDouble();

    currentLevel_s++;
    prevSentry_ = *getLastSentryPtr();
    *getLastSentryPtr() = this;

    if (logLevel_ == Level::Default)
        logLevel_ = Settings::defaultSentryLoggerLevel_s;
    else if (logLevel_ == Level::This || logLevel_ == Level::Parent)
        logLevel_ = prevSentry_->getLogLevel();
    if (relatedObj_ && logLevel_ != Level::Off)
        logobjects::registerObject(kind_, relatedObj_);
    else
        relatedObj_ = nullptr;
}

SentryLogger::~SentryLogger()
{
    // Exclude from chain
    if (*getLastSentryPtr() == this)
    {
        *getLastSentryPtr() = prevSentry_;
    }
    else
    {
        // something strange happens. deallocate not in order of allocation
        SentryLogger* cur = *getLastSentryPtr();
        for (; !cur; cur = cur->prevSentry_)
        {
            if (cur->prevSentry_ == this)
            {
                cur->prevSentry_ = cur->prevSentry_->prevSentry_;
                break;
            }
        }
    }
    // Decrease nested level
    currentLevel_s--;

    if (relatedObj_)
        logobjects::deregisterObject(kind_, relatedObj_);

    if (!isAllowed(Stage::Leave))
        return;

    std::string suffix;

    // Print leave message
    if (checkFlags(Flags::Timer))
    {
        double finTime = getCurTimestampAsDouble();
        suffix = TOSTR_FMT(". Processing time = {:.4}s", finTime - startTime_);
    }
    write(kind_, logLevel_, '<', getContextName(), "", ">> Leave scope", suffix);
}

SentryLogger* SentryLogger::getLast()
{
    return *getLastSentryPtr();
}

void SentryLogger::printStackTrace(StackTraceArgs args, Level level /*= Level::This*/)
{
    if (!Settings::stackTraceEnabled_s && !args.enforce)
        return;

    level = transformLogLevel(level);
    auto kind = (args.kind == Kind::Parent) ? kind_ : args.kind;
    if (!isAllowed(Stage::Enter, level, kind))
        return;

    auto& contextName = getContextName();

    // Ask for backtrace (and ignore this function)
    auto arrStr = ::tsv::debuglog::getStackTrace(args.depth, args.skip + 1);
    for (auto& s : arrStr)
        write(kind, level, ' ', contextName, "", s, {});
}

SentryLogger::StreamHelper SentryLogger::printEnterStreamy(std::string_view content /*=""*/)
{
    return StreamHelper(*this, getContextName(), content, Stage::Enter, logLevel_);
}

SentryLogger::StreamHelper SentryLogger::printStreamy(std::string_view content /*=""*/)
{
    return StreamHelper(*this, getContextName(), content, Stage::Event, logLevel_);
}

void SentryLogger::printEnter(std::string_view content /*=""*/)
{
    write(getContextName(), SentryLogger::Stage::Enter, logLevel_, content, "");
}

void SentryLogger::print(std::string_view content /*=""*/)
{
    write(getContextName(), SentryLogger::Stage::Event, logLevel_, content, "");
}

const std::string& SentryLogger::getContextName()
{
    if (prettyFuncName_)
    {
        if (!prettyFuncName_[0])
            contextName_ = findFuncName(prettyFuncName_, contextName_);
        prettyFuncName_ = nullptr;
    }
    return contextName_;
}

SentryLogger::Level SentryLogger::transformLogLevel(Level level) const
{
    if (level <= Level::Off)
        return level;
    if (level == Level::Default)
        return Settings::defaultSentryLoggerLevel_s;
    if (level == Level::This)
        return logLevel_;
    if (level == Level::Parent)
        return prevSentry_ ? prevSentry_->logLevel_ : logLevel_;
    return level;
}

void SentryLogger::setLogLevel(Level level)
{
    logLevel_ = transformLogLevel(level);
}

inline bool SentryLogger::checkFlags(Flags test) const
{
    return (static_cast<SentryLogger::EnumType_t>(flags_)
            & static_cast<SentryLogger::EnumType_t>(test));
}

void SentryLogger::setFlag(SentryLogger::Flags flags, bool enable /*= true*/)
{
    if (checkFlags(Flags::Timer))
    {
        if (enable)
        {
            if (!checkFlags(Flags::Timer))
                startTime_ = getCurTimestampAsDouble();
        }
        else if (checkFlags(Flags::Timer))
        {
            if (isAllowed(Stage::Event))
                write(getContextName(),
                      Stage::Event,
                      logLevel_,
                      TOSTR_FMT("Processed time: {:.4}s",
                                getCurTimestampAsDouble() - startTime_),
                      "");
        }
    }

    if (enable)
    {
        flags_ = static_cast<Flags>(static_cast<EnumType_t>(flags_)
                                    | static_cast<EnumType_t>(flags));
    }
    else
    {
        flags_ = static_cast<Flags>(static_cast<EnumType_t>(flags_)
                                    & ~static_cast<EnumType_t>(flags));
    }
}

bool SentryLogger::isAllowed(SentryLogger::Stage stage, SentryLogger::Level level, SentryLogger::Kind kind) const
{
    return level <= Settings::logLevel_s
           && ( checkFlags(Flags::Force)
                || (isStageAllowed(flags_, stage) && Settings::isKindAllowed(kind)));
}


SentryLogger::StreamHelper::StreamHelper(SentryLogger& log,
                                         const std::string_view contextName,
                                         const std::string_view body,
                                         SentryLogger::Stage stage,
                                         SentryLogger::Level level)
    : logger_(log)
    , contextName_(contextName)
    , body_(body)
    , stage_(stage)
{
    level_ = logger_.transformLogLevel(level);
}

SentryLogger::StreamHelper::~StreamHelper()
{
    static std::string emptyStr;

    if (level_ == SentryLogger::Level::Off)
        return;

    auto* stream = reinterpret_cast<std::ostringstream*>(stream_);
    logger_.write(contextName_, stage_, level_, body_, stream_ ? stream_->str() : emptyStr);
    delete stream_;
}

/*template<>
SentryLogger::StreamHelper& operator<< <SentryLogger::Level>(const SentryLogger::Level& level)*/
SentryLogger::StreamHelper& SentryLogger::StreamHelper::operator<<(const SentryLogger::Level& level)
{
    level_ = logger_.isAllowed(stage_, level, logger_.kind_) ? level : SentryLogger::Level::Off;
    return *this;
}


LastSentryLogger::LastSentryLogger(std::string_view extraContext /*=""*/)
{
    auto pos = extraContext.find_last_of("/\\");
    if (pos == std::string_view::npos)
        extraContextName_ = extraContext;
    else
        extraContextName_ = extraContext.substr(pos + 1);
}

const std::string& LastSentryLogger::getContextName()
{
    if (extraContextName_.empty() || !Settings::getPrintContextFlag())
        return SentryLogger::getLast()->getContextName();
    if (fullContextName_.empty())
    {
        fullContextName_ =
            SentryLogger::getLast()->getContextName() + ":" + extraContextName_;
    }
    return fullContextName_;
}

void LastSentryLogger::printStackTrace(StackTraceArgs args,
                                       SentryLogger::Level level /*= Level::This*/)
{
    // Duplicate content of SentryLogger::printStackTrace() because important to
    // be synced to have consistent behavior + apply modified getContext()
    if (!Settings::isStacktraceEnabled() && !args.enforce)
        return;

    auto* last = SentryLogger::getLast();
    level = last->transformLogLevel(level);
    auto kind = (args.kind==SentryLogger::Kind::Parent) ? last->getKind() : args.kind;
    if (!last->isAllowed(SentryLogger::Event, level, kind))
        return;

    auto& contextName = getContextName();

    // Ask for backtrace (and ignore this function)
    auto arrStr = ::tsv::debuglog::getStackTrace(args.depth, args.skip + 1);
    for (auto& s : arrStr)
        last->write(kind, level, ' ', contextName, "", s, {});
}

SentryLogger::StreamHelper LastSentryLogger::printStreamy(std::string_view content /*=""*/)
{
    auto* last = SentryLogger::getLast();
    return SentryLogger::StreamHelper(*last,
                                      getContextName(),
                                      content,
                                      SentryLogger::Stage::Event,
                                      SentryLogger::Level::This);
}

SentryLogger::StreamHelper LastSentryLogger::printEnterStreamy(std::string_view content /*=""*/)
{
    auto* last = SentryLogger::getLast();
    return SentryLogger::StreamHelper(*last,
                                      getContextName(),
                                      content,
                                      SentryLogger::Stage::Enter,
                                      SentryLogger::Level::This);
}

void LastSentryLogger::print(std::string_view content /*=""*/)
{
    auto* last = SentryLogger::getLast();
    last->write(getContextName(), SentryLogger::Stage::Event, last->getLogLevel(), content, "");
}

void LastSentryLogger::printEnter(std::string_view content /*=""*/)
{
    auto* last = SentryLogger::getLast();
    last->write(getContextName(), SentryLogger::Stage::Enter, last->getLogLevel(), content, "");
}

void SentryLogger::write(std::string_view contextName,
                         Stage stage,
                         Level level,
                         std::string_view body,
                         std::string_view streamBody)
{
    if (stage == SentryLogger::Stage::Enter)
    {
        write(kind_,
              level,
              '>',
              contextName,
              ">> Enter ",
              (streamBody.empty() && body.empty()) ? std::string("scope") : body,
              streamBody);
    }
    else
    {
        // Stage:Event
        write(kind_, level, ' ', contextName, "", body, streamBody);
    }
}

void SentryLogger::write(Kind kind,
                         Level level,
                         char nestedSym,
                         std::string_view contextName,
                         std::string_view prefix,
                         std::string_view body,
                         std::string_view streamBody)
{
    if (!Settings::outputHandler_s)
        return;
    if (body.empty() && streamBody.empty())
        return;

    std::string fmtStr;
    fmtStr.reserve(50);

    // Format ""{0_LoggerPrefix}{1_Level:0>2}{2_nestedSym:>>{1_Level}}{3_KindName}{{{4_Context}}} {5_Prefix}{6_Body}{7_StreamBody}"
    fmtStr.append("{0}");
    if (Settings::isNestedLevelMode_s)
        fmtStr.append("{1:0>2}{2:").append(1, nestedSym).append(">{1}}");
    if (Settings::printKindFlag_s)
        fmtStr.append("{3}");
    if (Settings::printContextFlag_s && !contextName.empty())
        fmtStr.append("{{{4}}}");
    fmtStr.append(" {5}{6}{7}");

    auto s = TOSTR_FMT(fmtStr,
                       Settings::loggerPrefix_s,        // 0
                       currentLevel_s,                  // 1
                       nestedSym,                       // 2 
                       kindNamesStr[static_cast<EnumType_t>(kind)],
                       contextName,                     // 4
                       prefix,                          // 5
                       body,                            // 6
                       streamBody);                     // 7
    Settings::outputHandler_s(level, s);
}


namespace logobjects
{

namespace
{
// @note: argument validation is caller responsibility
auto& allowedObjectSet(SentryLogger::Kind kind)
{
    static std::vector<std::unordered_set<const void*>> objSet(
        static_cast<SentryLogger::EnumType_t>(SentryLogger::Kind::NumberOfKinds));
    return objSet[static_cast<SentryLogger::EnumType_t>(kind)];
}
}  // namespace


Guard::Guard(SentryLogger::Kind kind, const void* objPtr, bool registerObject /* = true*/)
    : kind_(kind)
    , objPtr_(objPtr)
{
    if (registerObject)
        ::tsv::debuglog::logobjects::registerObject(kind, objPtr);
}

Guard::~Guard()
{
    deregisterObject(kind_, objPtr_);
}

void setAllowAll(bool flag, SentryLogger::Kind kind /* = SentryLogger::Kind::Default*/)
{
    if (kind >= SentryLogger::Kind::NumberOfKinds)
        return;
    auto& objSet = allowedObjectSet(kind);
    if (flag)
        objSet.insert(nullptr);
    else
        objSet.erase(nullptr);
}

void registerObject(SentryLogger::Kind kind, const void* objPtr)
{
    if (kind >= SentryLogger::Kind::NumberOfKinds || !objPtr)
        return;
    auto& objSet = allowedObjectSet(kind);
    objSet.insert(objPtr);
}

void deregisterObject(SentryLogger::Kind kind, const void* objPtr)
{
    if (kind >= SentryLogger::Kind::NumberOfKinds || !objPtr)
        return;
    auto& objSet = allowedObjectSet(kind);
    objSet.erase(objPtr);
}

// Return true if objPtr is registered for given kind or its mode is "AllowAll" 
// (by default it isn't)
bool isAllowedObj(const void* objPtr,
                  SentryLogger::Kind kind /*= SentryLogger::Kind::Default*/)
{
    if (kind >= SentryLogger::Kind::NumberOfKinds || !objPtr)
        return false;
    auto& objSet = allowedObjectSet(kind);
    if (objSet.empty())
        return false;
    if (objSet.find(objPtr) != objSet.end())
        return true;
    if (objSet.find(nullptr) != objSet.end())
        return true;
    return false;
}

}  // namespace logobjects

}  // namespace tsv::debuglog
