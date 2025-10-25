/**
  Purpose: Scope/function enter/exit and regular logging
  Author: Taranenko Sergey
  Date: 02-Aug-2017 ... 19-Oct-2024
  License: BSD. See License.txt
*/

// Always enforce flags with 1 here because we need full class declarations here
#define DEBUG_LOGGING 1

#include "debuglog_settings.h"
#include "debugresolve.h"

#include "tostr_fmt_include.h"
#include <chrono>
#include <unordered_set>

namespace
{
void defaultLoggerHandler([[maybe_unused]]SentryLogger::Level level, [[maybe_unused]]SentryLogger::Kind kind, [[maybe_unused]]std::string_view line)
{
    //if (level <= Settings::getLogLevel()) fmt::print("[DBG]{}\n", line);
}
}

//#define LOCAL_DEBUG(...) __VA_ARGS__
#define LOCAL_DEBUG(...)

namespace tsv::debuglog
{

/**
 *   STATIC VARIABLES + SETTINGS
 */
bool Settings::stackTraceEnabled_s = true;
bool Settings::isNestedLevelMode_s = true;   // if true, then show graphically hierarchy
bool Settings::printContextFlag_s = true;
bool Settings::printKindFlag_s = true;
// do not change this initial value over Level::Off
SentryLogger::Level Settings::logLevel_s = SentryLogger::Level::Info;
// do not change this initial value over Level::Off
SentryLogger::Level Settings::defaultSentryLoggerLevel_s = SentryLogger::Level::Info;
SentryLogger::Level Settings::watchLogLevel_s  = SentryLogger::Level::Default;
Settings::OutputHandler Settings::outputHandler_s = defaultLoggerHandler;
std::string Settings::loggerPrefix_s = "";
std::vector<std::string> Settings::cutoffNamespaces_s;

int SentryLogger::currentLevel_s  = 0 ;

/**
 * Local Helpers
 */
namespace
{

//TODO: singletone to make sure it already initialized

// Prefixes which are printed for each kind
std::vector<std::string> kindNamesStr =
{
    ""  // Default - no special mark
        //,"[Sample]"   // Sample correspondance kind to its output signature
};

bool isStageAllowed(SentryLogger::Flags flags, SentryLogger::Stage stage)
{
    // trick: stages match to corresponding Suppress flags
    return !(static_cast<SentryLogger::EnumType_t>(flags)
             & static_cast<SentryLogger::EnumType_t>(stage));
}

bool startsWith(std::string_view base, std::string_view lookup )
{
    return ( base.size() >= lookup.size() ) && (base.substr(0,lookup.size()) == lookup );
}

inline bool isKindValid(sentry_enum::Kind kind)
{
    return (kind > SentryLogger::Kind::Off
            && static_cast<sentry_enum::EnumType_t>(kind) < getNumberOfKinds());
}

}   // namespace anonymous


/**
 * Weak linked callback
 */

// Default implementation
// @note It should be replaced in the user code if enum Kind is redefined
sentry_enum::EnumType_t __attribute__((weak)) getNumberOfKinds() 
{
    return static_cast<sentry_enum::EnumType_t>(sentry_enum::Kind::NumberOfKinds);
}


/**
    Settings
*/
int* Settings::getKindsStateArray()
{
    // Default everything is turned off
    static std::vector<int> kindsStateArray(
        static_cast<std::vector<int>::size_type>(getNumberOfKinds() + 1));
    LOCAL_DEBUG(std::string arr;
                for (int i
                     : kindsStateArray) arr += "." + std::to_string(i);
                fmt::print("<{}>", arr);)
    return kindsStateArray.data();
}

bool Settings::isKindAllowed(SentryLogger::Kind kind)
{
    return (kind > SentryLogger::Kind::Off
            && static_cast<EnumType_t>(kind) < getNumberOfKinds()
            && !!getKindsStateArray()[static_cast<EnumType_t>(kind)]);
}

int Settings::getLoggerKindState(SentryLogger::Kind kind)
{
    LOCAL_DEBUG( fmt::print("getKind_{}\n", static_cast<int>(kind)); )
    if (static_cast<EnumType_t>(kind) < getNumberOfKinds())
        return getKindsStateArray()[static_cast<EnumType_t>(kind)];
    return 0;
}

SentryLogger::Level Settings::getLoggerKindStateAsLevel(SentryLogger::Kind kind)
{
    EnumType_t rv = static_cast<EnumType_t>(Level::Off);
    if (static_cast<EnumType_t>(kind) < getNumberOfKinds())
        rv = static_cast<EnumType_t>(getKindsStateArray()[static_cast<EnumType_t>(kind)]);
    if (rv > static_cast<EnumType_t>(Level::Off))
        rv = static_cast<EnumType_t>(Level::Off);
    return static_cast<SentryLogger::Level>(rv);
}

void Settings::setLoggerKindState(SentryLogger::Kind kind, bool enableFlag /*=true*/)
{
    if (isKindValid(kind))
        getKindsStateArray()[static_cast<EnumType_t>(kind)] = enableFlag;
    LOCAL_DEBUG( fmt::print(FMT_STRING("SETKIND_B_{}={}/"), static_cast<int>(kind), enableFlag); getKindsStateArray(); fmt::print("\n"); )
}

void Settings::setLoggerKindState(SentryLogger::Kind kind, int value)
{
    if (isKindValid(kind))
        getKindsStateArray()[static_cast<EnumType_t>(kind)] = value;
    LOCAL_DEBUG( fmt::print(FMT_STRING("SETKIND_I_{}={}/"), static_cast<int>(kind), value); getKindsStateArray(); fmt::print("\n"); )
}

void Settings::setLoggerKindState(SentryLogger::Kind kind, SentryLogger::Level value)
{
    if (isKindValid(kind))
        getKindsStateArray()[static_cast<EnumType_t>(kind)] = static_cast<int>(value);
}

void Settings::setLogLevel(SentryLogger::Level level)
{
    if (level == Level::Default)
    {
        if (defaultSentryLoggerLevel_s > Level::Off)
            defaultSentryLoggerLevel_s = Level::Off;
        level = defaultSentryLoggerLevel_s;
    }
    LOCAL_DEBUG( fmt::print(FMT_STRING("setLogLevel {}->{}\n"), static_cast<int>(logLevel_s), static_cast<int>(level));)
    if (level <= Level::Off)
        logLevel_s = level;
}

void Settings::setWatchLogLevel(Level level)
{
    if (level == Level::Default)
    {
        if (defaultSentryLoggerLevel_s > Level::Off)
            defaultSentryLoggerLevel_s = Level::Off;
        level = defaultSentryLoggerLevel_s;
    }
    if (level <= Level::Off)
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

SentryLogger::Level Settings::getDefaultLevel()
{
    return defaultSentryLoggerLevel_s;
}

void Settings::setDefaultLogLevel(SentryLogger::Level level)
{
    LOCAL_DEBUG( fmt::print(FMT_STRING("setDefaultLogLevel {}->{}\n"), static_cast<int>(logLevel_s), static_cast<int>(level));)
    if (level <= Level::Off)
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

std::vector<std::string>& Settings::cutoffNamespaces()
{
    return cutoffNamespaces_s;
}

void Settings::setCutoffNamespaces(std::vector<std::string> arr)
{
    cutoffNamespaces_s = std::move(arr);
}

void Settings::setKindNames(const std::vector<KindNamePair>& names)
{
    kindNamesStr.resize( static_cast<EnumType_t>(getNumberOfKinds()) );
    for (auto p : names)
    {
        if (static_cast<EnumType_t>(p.kind_) < getNumberOfKinds())
            kindNamesStr[ static_cast<unsigned>(p.kind_)] = p.name_;
    }

    LOCAL_DEBUG( for (std::size_t i=0; i< kindNamesStr.size(); i++) fmt::print("kind {} -> {}\n", i, kindNamesStr[i]); )
}

Settings::Operation::Operation()
{
    // initialize with values which do nothing
    type_ = Type::SetLogLevel;
    enumValue_ = static_cast<SentryLogger::EnumType_t>(SentryLogger::Level::Parent);
}

Settings::Operation::Operation(SentryLogger::Kind kind, int enableFlag)
{
    type_ = Type::SetKind;
    enumValue_ = static_cast<SentryLogger::EnumType_t>(kind);
    value_ = enableFlag;
}

// @note: no default value to prevent ambiguilty copy and main ctor for Operation({{smth}});
Settings::Operation::Operation(SentryLogger::Level level, Type type)
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
        LOCAL_DEBUG(fmt::print("set(type_{}) = {}\n", static_cast<int>(op.type_), static_cast<int>(op.enumValue_));)
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
            //@todo -- should be int
            bool flag = op.value_;
            LOCAL_DEBUG(fmt::print("temp_set_kind{}={}\n", static_cast<int>(kind), flag);)
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

        LOCAL_DEBUG(fmt::print("temp_set_level {}->{}\n", static_cast<int>(curLevel), static_cast<int>(op.enumValue_));)
            auto newLevel = static_cast<SentryLogger::Level>(op.enumValue_);
            if (rollbackOnlyChanged && (curLevel == newLevel || newLevel > SentryLogger::Level::Off))
                continue;
            operations_[idx++] = Operation(curLevel, op.type_);

            if (op.type_==Operation::SetWatchLogLevel) 
                Settings::setWatchLogLevel(newLevel);
            else if (op.type_==Operation::SetDefaultLogLevel) 
                Settings::setDefaultLogLevel(newLevel);
            else
                Settings::setLogLevel(newLevel);
        }
    }
    operations_.resize(idx);
}


Settings::TemporarySettings::~TemporarySettings()
{
    LOCAL_DEBUG(fmt::print("end_of_temp\n");)
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
    double t =  duration_cast<duration<double>>(now.time_since_epoch()).count();
    return t;
}

// Aux function to controllable lifetime and initialize of "last_s"
SentryLogger** getLastSentryPtr()
{
    static SentryLogger* last_s = SentryLogger::getRoot();
    return &last_s;
}

} // anonymous namespace

/**
 * extract full name from the __PRETTY_FUNCTION__
 */
std::string_view extractFuncName(std::string_view prettyFnName)
{
    // Find the last space before the opening parenthesis
    auto parenthPos = prettyFnName.rfind('(');
    if (parenthPos == std::string_view::npos)
        return std::string_view{};

    auto lastSpace = prettyFnName.rfind(' ', parenthPos);
    if (lastSpace == std::string_view::npos)
        return std::string_view{};

    // Extract everything after the last space up to the opening parenthesis
    return prettyFnName.substr(lastSpace + 1, parenthPos - lastSpace - 1);
}


/**
  *   Main section
  */


// Private ctor for root
SentryLogger::SentryLogger(RootTag)
{
    contextName_ = "core";
    prettyName_ = nullptr;
    flags_ = SentryLogger::Flags::SuppressBorders;
    // for "root record the initial default level is assigned but could be overriden later
    logLevel_ = Settings::defaultSentryLoggerLevel_s;
    mainLogLevel_ = logLevel_;
    kind_ = SentryLogger::Kind::Default;
    prevSentry_ = nullptr;
}

// Main ctor
SentryLogger::SentryLogger(const char* prettyName,
                           std::string_view name /*=""*/,
                           InitArgs args /*={}*/)
    : flags_(args.flags)
    , mainLogLevel_(args.enabled ? transformLogLevel(args.level) : Level::Off)
    , kind_(static_cast<EnumType_t>(args.kind) >= getNumberOfKinds() ? Kind::Off
                                                                     : args.kind)
    , relatedObj_(args.object)
    , contextName_(name)
    , prettyName_(prettyName)
{
    if (kind_ < Kind::Off)
         kind_ = Kind::Off;

    logLevel_ = mainLogLevel_;
    LOCAL_DEBUG( fmt::print(FMT_STRING("ctor SENTRY - {}|{} | enabled_{}| kind_{}->{}|level_{}->{}\n"), name, prettyName?prettyName:"null", args.enabled, static_cast<int>(args.kind), static_cast<int>(kind_), static_cast<int>(args.level), static_cast<int>(logLevel_)); )

    if (!prettyName_ && checkFlags(Flags::AppendContextName))
         contextName_ = getLast()->getContextName() + "--" + contextName_;

    // Do not place disabled to the sentries stack.
    // That is the way to make it invisible
    if (args.enabled)
    {
        currentLevel_s++;
        prevSentry_ = *getLastSentryPtr();
        *getLastSentryPtr() = this;
    }
    else
    {
        setFlag(Flags::SuppressBorders);
    }

    if (logLevel_ == Level::Default)
        logLevel_ = Settings::defaultSentryLoggerLevel_s;
    else if (logLevel_ == Level::This || logLevel_ == Level::Parent)
        logLevel_ = prevSentry_->getLogLevel();

    if (relatedObj_ && logLevel_ != Level::Off)
        logobjects::registerObject(kind_, relatedObj_);
    else
        relatedObj_ = nullptr;

    // at the end to minimize impact
    if (checkFlags(Flags::Timer))
        startTime_ = getCurTimestampAsDouble();
}

// Ctor of "silent" sentry
SentryLogger::SentryLogger(Level level,
                           std::string_view name,
                           bool appendContextFlag /*= true*/,
                           Kind kind /*= Default*/)
    : SentryLogger::SentryLogger(nullptr, name, {kind, level, Flags::SuppressBorders})
{
    flags_ = static_cast<Flags>(flags_ | (appendContextFlag ? Flags::AppendContextName : Flags::Default));
}

SentryLogger::~SentryLogger()
{
    int decrementLevel = 0;

    // Exclude from chain
    if (*getLastSentryPtr() == this)
    {
        decrementLevel = 1;
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
                decrementLevel = 1;
                cur->prevSentry_ = cur->prevSentry_->prevSentry_;
                break;
            }
        }
    }

    if (relatedObj_)
        logobjects::deregisterObject(kind_, relatedObj_);

    if (isAllowed(Stage::Leave))
    {
        std::string suffix;

        // Print leave message
        if (checkFlags(Flags::Timer))
        {
            double finTime = getCurTimestampAsDouble();
            suffix = TOSTR_FMT(". Processing time = {:.4f}s", finTime - startTime_);
        }
        if (!returnValueStr_.empty())
            suffix += TOSTR_FMT(". RV = {}", returnValueStr_);
        write(kind_, logLevel_, '<', getContextName(), "", ">> Leave scope", suffix);
    }

    currentLevel_s -= decrementLevel;
    LOCAL_DEBUG( fmt::print(FMT_STRING("dtor SENTRY - {} | kind_{}|level_{}|nestLevel={}\n"), getContextName(), static_cast<int>(kind_), static_cast<int>(logLevel_), currentLevel_s); )
}

SentryLogger* SentryLogger::getRoot()
{
    static SentryLogger rootLogger{RootTag{}};
    return &rootLogger;
}

SentryLogger* SentryLogger::getLast()
{
    return *getLastSentryPtr();
}

void SentryLogger::printStackTrace()
{
    return printStackTrace({}, Level::This);
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

void SentryLogger::print(std::string_view content /*=""*/)
{
    write(getContextName(), SentryLogger::Stage::Event, logLevel_, content, "");
}

const std::string& SentryLogger::getContextName()
{
    if (prettyName_)
    {
        if (contextName_.empty())
        {
            if (prettyName_[0]=='|')
            {
                // path given
                std::string_view s{prettyName_};
                auto pos = s.rfind("/");
                if (pos != std::string_view::npos)
                    contextName_ = s.substr(pos+1);
            }
            else if (prettyName_[0])
            {
                // Function Name given
                contextName_ = extractFuncName(prettyName_);
                for (const std::string& s : Settings::cutoffNamespaces())
                {
                    if (startsWith(contextName_,s))
                    {
                        contextName_ = std::string_view(contextName_).substr(s.size());
                        break;
                    }
                }
            }
        }
        if (checkFlags(Flags::AppendContextName))
            contextName_ = prevSentry_->getContextName() + "--" + contextName_;
        prettyName_ = nullptr;
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
    mainLogLevel_ = transformLogLevel(level);
    logLevel_ = mainLogLevel_;
}

void SentryLogger::setReturnValueStr(std::string rv)
{
    returnValueStr_ = rv;
}

inline bool SentryLogger::checkFlags(Flags test) const
{
    return (static_cast<SentryLogger::EnumType_t>(flags_)
            & static_cast<SentryLogger::EnumType_t>(test));
}

void SentryLogger::setFlag(SentryLogger::Flags flags, bool enable /*= true*/)
{
    if (static_cast<SentryLogger::EnumType_t>(flags)
        & static_cast<SentryLogger::EnumType_t>(Flags::Timer))
    {
        if (enable)
        {
            if (!checkFlags(Flags::Timer))
                startTime_ = getCurTimestampAsDouble();
        }
        else if (checkFlags(Flags::Timer))
        {
            if (isAllowed(Stage::Event))
            {
                double fin = getCurTimestampAsDouble();
                write(getContextName(),
                      Stage::Event,
                      logLevel_,
                      TOSTR_FMT("Processed time: {:.4f}s",
                                fin - startTime_),
                      "");
            }
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

bool SentryLogger::isAllowedStage(SentryLogger::Stage stage) const
{
    LOCAL_DEBUG( fmt::print(FMT_STRING("ISSTAGE=force_{}/stage_{}\n"), checkFlags(Flags::Force), isStageAllowed(flags_, stage)); )
    return checkFlags(Flags::Force) || isStageAllowed(flags_, stage);
}

bool SentryLogger::isAllowed(SentryLogger::Stage stage, SentryLogger::Level level, SentryLogger::Kind kind) const
{
    LOCAL_DEBUG( fmt::print(FMT_STRING("IS=level_{}/{}|force_{}|stage_{}|kind_{}/{}\n"), level, Settings::logLevel_s, checkFlags(Flags::Force), isStageAllowed(flags_, stage), static_cast<unsigned>(kind), Settings::isKindAllowed(kind));)    
    return level <= Settings::logLevel_s
           && ( checkFlags(Flags::Force)
                || (isStageAllowed(flags_, stage) && Settings::isKindAllowed(kind)));
}

bool SentryLogger::isAllowedAndSetTempLevel(SentryLogger::Stage stage, SentryLogger::Level level)
{
    LOCAL_DEBUG( fmt::print(FMT_STRING("IS&SET=level_{}/{}|force_{}|stage_{}|kind_{}/{}\n"), level, Settings::logLevel_s, checkFlags(Flags::Force), isStageAllowed(flags_, stage), static_cast<unsigned>(kind_), Settings::isKindAllowed(kind_)); )
    if (level > Settings::logLevel_s)
        return false;

    if (checkFlags(Flags::Force)
        || (isStageAllowed(flags_, stage) && Settings::isKindAllowed(kind_)))
    {
        // Only update current loglevel only for allowed because it is reseted in write()
        logLevel_ = level;
        return true;
    }
    return false;
}

SentryLogger::EnterHelper::EnterHelper(std::string_view content /*=""*/)
{
    // We can rely on the SentryLogger object (to which this helper is related), never being hidden in the stack.
    // That is because hiding could happen for .enabled=false, and then the "if" condition in the macro will not pass the control flow here.
    SentryLogger* last = SentryLogger::getLast();
    last->write(last->getContextName(), SentryLogger::Stage::Enter, last->logLevel_, content, "");
}

// Isolated function to make possible to extend its logic if needed in future
const std::string& LastSentryLogger::getContextName()
{
    return SentryLogger::getLast()->getContextName();
}

[[gnu::noinline]] 
void LastSentryLogger::printStackTrace()
{
    return printStackTrace({-1, 1}, Level::This);
}

[[gnu::noinline]] 
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

void LastSentryLogger::print(std::string_view content /*=""*/)
{
    auto* last = SentryLogger::getLast();
    last->write(getContextName(), SentryLogger::Stage::Event, last->getLogLevel(), content, "");
}

void SentryLogger::write(std::string_view contextName,
                         Stage stage,
                         Level level,
                         std::string_view body,
                         std::string_view suffix)
{
    if (stage == SentryLogger::Stage::Enter)
    {
        write(kind_,
              level,
              '>',
              contextName,
              ">> Enter ",
              (suffix.empty() && body.empty()) ? std::string("scope") : body,
              suffix);
    }
    else
    {
        // Stage:Event
        write(kind_, level, ' ', contextName, "", body, suffix);
    }
}

void SentryLogger::write(Kind kind,
                         Level level,
                         char nestedSym,
                         std::string_view contextName,
                         std::string_view prefix,
                         std::string_view body,
                         std::string_view suffix)
{
    if (!Settings::outputHandler_s)
        return;
    if (body.empty() && suffix.empty())
        return;

    LOCAL_DEBUG(fmt::print("write|{}|{}|{}|{}|\n", contextName, prefix, body, suffix);)
    std::string nested;
    if (Settings::isNestedLevelMode_s)
    {
        std::string fmtStr;
        fmtStr.append("{0:0>2}{1:").append(1, nestedSym).append(">{0}}");
        nested = TOSTR_FMT(fmtStr, currentLevel_s, nestedSym);
    }

    // Format ""{0_LoggerPrefix}{1_NestedCombo}{2_KindName}{{{3_Context}}}{4_Prefix}{5_Body}{6_Suffix}"
    auto s = TOSTR_FMT("{0}{1}{2}{{{3}}}{4}{5}{6}",
                       Settings::loggerPrefix_s,                           // 0
                       nested,                                             // 1
                       (Settings::printKindFlag_s) ? kindNamesStr[static_cast<EnumType_t>(kind)] : std::string_view(),  // 2
                       (Settings::printContextFlag_s && !contextName.empty()) ? contextName : "",       // 3
                       prefix,                                             // 4
                       body,                                               // 5
                       suffix);                                            // 6

    Settings::outputHandler_s(level, kind, s);
}


namespace logobjects
{

namespace
{
// @note: argument validation is caller responsibility
auto& allowedObjectSet(SentryLogger::Kind kind)
{
    static std::vector<std::unordered_set<const void*>> objSet(getNumberOfKinds());
    return objSet[static_cast<SentryLogger::EnumType_t>(kind)];
}
}  // namespace


Guard::Guard(const void* objPtr,
             SentryLogger::Kind kind /*= SentryLogger::Kind::Default*/,
             bool registerObject /*= true*/)
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
    if (!isKindValid(kind))
        return;
    auto& objSet = allowedObjectSet(kind);
    LOCAL_DEBUG( fmt::print("register_all_{} {}\n", static_cast<int>(kind), flag) );
    if (flag)
        objSet.insert(nullptr);
    else
        objSet.erase(nullptr);
}

void registerObject(SentryLogger::Kind kind, const void* objPtr)
{
    if (!isKindValid(kind) || !objPtr)
        return;
    LOCAL_DEBUG( fmt::print("register_{} {}\n", static_cast<int>(kind), objPtr) );
    auto& objSet = allowedObjectSet(kind);
    objSet.insert(objPtr);
}

void deregisterObject(SentryLogger::Kind kind, const void* objPtr)
{
    if (!isKindValid(kind) || !objPtr)
        return;
    LOCAL_DEBUG( fmt::print("DEregister_{} {}\n", static_cast<int>(kind), objPtr) );
    auto& objSet = allowedObjectSet(kind);
    objSet.erase(objPtr);
}

// Return true if objPtr is registered for given kind or its mode is "AllowAll" 
// (by default it isn't)
bool isAllowedObj(const void* objPtr,
                  SentryLogger::Kind kind /*= SentryLogger::Kind::Default*/)
{
    if (!isKindValid(kind) || !objPtr)
        return false;
    LOCAL_DEBUG( fmt::print("isAllowedObj{} {}\n", static_cast<int>(kind), objPtr) );
    auto& objSet = allowedObjectSet(kind);
    //LOCAL_DEBUG( for (auto k_ : objSet) { fmt::print("kind_{}: {}\n",static_cast<int>(kind),fmt::join(objSet[static_cast<SentryLogger::EnumType_t>(kind)],",")); } )
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
