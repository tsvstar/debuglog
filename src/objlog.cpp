/**
  Purpose: Tracking objects lifecycle
  Author: Taranenko Sergey (tsvstar@gmail.com)
  Date: 02-Aug-2017 ... 19-Oct-2024
  License: BSD. See License.txt
*/

// Enforce flag to correct declaration and definition
#define DEBUG_OBJLOG 1
#define DEBUG_LOGGING 1

#include "debuglog_settings.h"
#include "objlog.h"
#include <unordered_map>
#include <map>
#include "tostr_fmt_include.h"

namespace
{
    typedef std::unordered_map<const void*, int> UnordMapPtrInt_t;

    // Auxiliary containers, which store info about object allocations
    struct ObjMap
    {
        // Ordered map to sorted output of printTrackedPtr
        // ["className"][object_ptr] = counter
        std::map<std::string, UnordMapPtrInt_t > allObjectsMap;

        // ["className"] = counter_s
        std::map<std::string, int> objectsCounter;
    };

    ObjMap& getInstance()
    {
        static ObjMap instance;
        return instance;
    }
}

namespace tsv::debuglog
{

// Setting
bool ObjLoggerImpl::includeContextName_s = true;

namespace
{

void printObjEvent(sentry_enum::Kind kind, sentry_enum::Level level, std::string_view content)
{
    auto* last = SentryLogger::getLast();
    last->write(kind,
                last->transformLogLevel(level),
                ' ',
                ObjLoggerImpl::includeContextName_s ? last->getContextName() : "",
                "",
                content,
                {});
}

void printStackTrace(int depth, sentry_enum::Level level, SentryLogger::Kind kind)
{
    if (depth)
    {
        SentryLogger::getLast()->printStackTrace({/*.depth=*/depth,
                                                  /*.skip=*/2,
                                                  /*enforce*/ false,
                                                  /*.kind=*/kind},
                                                 level);
    }
}

inline ptrdiff_t getOffs(const void* loggerSelf,
                         const void* ptr,
                         sentry_enum::Level level,
                         sentry_enum::Kind kind)
{
    if (!ptr || level > Settings::getLogLevel() || !Settings::isKindAllowed(kind))
        return -1;

    return (reinterpret_cast<const char*>(loggerSelf)
            - reinterpret_cast<const char*>(ptr));
}

}   // namespace anonymous

// -TODO: not sure that signed offs_ is safe. 
// - it is. a) pointer ariphm only defined for the same block of memory and normal usage of objlogger assume that ptr>this

ObjLoggerImpl::ObjLoggerImpl(const void* ptr,
                             const char* className,
                             int depth /*=0*/,
                             const char* comment /*=""*/,
                             sentry_enum::Level level /*=Default*/,
                             sentry_enum::Kind kind /*=Tracked*/)
    : depth_(depth)
    , kind_(kind)
    , level_(level <= sentry_enum::Level::Off ? level : Settings::getDefaultLevel())
    , offs_(getOffs(this, ptr, level_, kind))
    , className_(className)
{
    // Untrackable object (due to level, kind, given ptr)
    if (offs_ < 0)
        return;

    getInstance().allObjectsMap[className_][ptr]++;
    getInstance().objectsCounter[className_]++;
    printObjEvent(kind_,
                  level_,
                  TOSTR_FMT("[obj:{}:{}] create {:p} {}",
                            className,
                            getInstance().objectsCounter[className],
                            ptr,
                            comment ? comment : ""));
    printStackTrace(depth_, level_, kind_);
}

ObjLoggerImpl::ObjLoggerImpl(const void* ptr,
                             const void* copied_from,
                             const char* className,
                             int depth,
                             const char* comment,
                             sentry_enum::Level level,
                             sentry_enum::Kind kind)
    : depth_(depth)
    , kind_(kind)
    , level_(level <= sentry_enum::Level::Off ? level : Settings::getDefaultLevel())
    , offs_(getOffs(this, ptr, level_, kind))
    , className_(className)
{
    // Untrackable object (due to level, kind, given ptr)
    if (offs_ < 0)
        return;

    getInstance().allObjectsMap[className_][ptr]++;
    getInstance().objectsCounter[className_]++;
    printObjEvent(kind_,
                  level_,
                  TOSTR_FMT("[obj:{}:{}] {:p} copy_ctor({:p}) {}",
                            className,
                            getInstance().objectsCounter[className_],
                            ptr,
                            copied_from,
                            comment ? comment : ""));
    printStackTrace(depth_, level_, kind_);
}

ObjLoggerImpl::ObjLoggerImpl(const ObjLoggerImpl& obj)
    : depth_(obj.depth_)
    , kind_(obj.kind_)
    , level_(obj.level_)
    , offs_(obj.offs_)
    , className_(obj.className_)
{
    if (offs_ < 0)
        return;

    const void* ptr = reinterpret_cast<const char*>(this) - offs_;
    const void* copied_from = reinterpret_cast<const char*>(&obj) - offs_;

    getInstance().allObjectsMap[className_][ptr]++;
    getInstance().objectsCounter[className_]++;
    printObjEvent(kind_,
                  level_,
                  TOSTR_FMT("[obj:{}:{}] {:p} copy_ctor_dflt({:p})",
                            className_,
                            getInstance().objectsCounter[className_],
                            ptr,
                            copied_from));
    printStackTrace(depth_, level_, kind_);
}

ObjLoggerImpl::ObjLoggerImpl(const ObjLogger&& obj)
    : depth_(obj.depth_)
    , kind_(obj.kind_)
    , level_(obj.level_)
    , offs_(obj.offs_)
    , className_(obj.className_)
{
    if (offs_ < 0)
        return;

    const void* ptr = reinterpret_cast<const char*>(this) - offs_;
    const void* copied_from = reinterpret_cast<const char*>(&obj) - offs_;

    getInstance().allObjectsMap[className_][ptr]++;
    getInstance().objectsCounter[className_]++;
    printObjEvent(kind_,
                  level_,
                  TOSTR_FMT("[obj:{}:{}] {:p} copy_ctor_move({:p})",
                            className_,
                            getInstance().objectsCounter[className_],
                            ptr,
                            copied_from));
    printObjEvent(kind_,
                  level_,
                  TOSTR_FMT("[obj:{}:{}] {:p} become unitialized",
                            className_,
                            getInstance().objectsCounter[className_],
                            copied_from));
    printStackTrace(depth_, level_, kind_);
}

ObjLoggerImpl& ObjLoggerImpl::operator=(const ObjLoggerImpl& obj)
{
    if (const_cast<const ObjLoggerImpl*>(this) == &obj)
        return *this;
    if (offs_ < 0)
        return *this;

    const void* ptr = reinterpret_cast<const char*>(this) - offs_;
    const void* from = reinterpret_cast<const char*>(&obj) - obj.offs_;

    if (offs_ != obj.offs_ || className_ != obj.className_)
    {
        printObjEvent(kind_,
                      level_,
                      TOSTR_FMT("[obj] INCONSISTENCE operator=() -> className={}|{} offs={}|{}",
                                className_,
                                obj.className_,
                                offs_,
                                obj.offs_));
    }
    else
    {
        printObjEvent(kind_,
                      level_,
                      TOSTR_FMT("[obj:{}:{}] {:p} operator=({:p})",
                                className_,
                                getInstance().objectsCounter[className_],
                                ptr,
                                from));
        // depth could be different
        depth_ = obj.depth_;
    }

    printStackTrace(depth_, level_, kind_);

    // Do nothing real work because both sides are exists (and so registered)
    return *this;
}


ObjLoggerImpl::~ObjLoggerImpl()
{
    if (offs_ < 0)
        return;

    void* objPtr = reinterpret_cast<char*>(this) - offs_;
    UnordMapPtrInt_t& obj = getInstance().allObjectsMap[className_];
    UnordMapPtrInt_t::iterator objCntr = obj.find(objPtr);
    int& classCntr = getInstance().objectsCounter[className_];

    if (objCntr == obj.end())
    {
        printObjEvent(kind_,
                      level_,
                      TOSTR_FMT("[obj:{}:{}] destroy {:p}. ERROR: not registered pointer",
                                className_,
                                classCntr,
                                objPtr));
    }
    else
    {
        classCntr--;
        printObjEvent(kind_,
                      level_,
                      TOSTR_FMT("[obj:{}:{}] destroy {:p}",  //
                                className_,
                                classCntr,
                                objPtr));
        if (objCntr->second > 1)
            objCntr->second--;
        else
            obj.erase(objCntr);
    }

    printStackTrace(depth_, level_, kind_);
}

/** Print all or exact class pointers
 * 
 *  className  = if nullptr print all tracked pointers
 *               otherwise only tracked pointers of given class
 *  level, kindOutput = print parameters (no affect to filtering)
 */
void ObjLoggerImpl::printTrackedPtr(const char* className /*= nullptr */,
                                    sentry_enum::Level level /*= Default*/,
                                    sentry_enum::Kind kindOutput /*= Tracked*/)
{
    // todo: add arg "filterByKind". if ==Parent then no filtering, otherwise filter by sentry.kind_ ??
    auto& allObjectsMap = getInstance().allObjectsMap;
    if (!className)
    {
        printObjEvent(kindOutput, level, " [obj] Print all active pointers");
        for (auto& tmp : allObjectsMap)
            printTrackedPtr(tmp.first.c_str(), level, kindOutput);
        return;
    }

    auto it = allObjectsMap.find(className);
    if (it == allObjectsMap.end())
    {
        printObjEvent(kindOutput,
                      level,
                      TOSTR_FMT(" [obj] Print {}: not found such class", className));
        return;
    }

    std::string output;
    int cntr = 0;
    auto& ptrMap = it->second;
    for (const auto& ptr : ptrMap)
    {
        if (!ptr.second)
            continue;

        if (ptr.second == 1)
            output.append(TOSTR_FMT("{}{:p}", cntr ? "," : "", ptr.first));
        else
            output.append(
                TOSTR_FMT("{}{:p}({})", cntr ? "," : "", ptr.first, ptr.second));

        cntr++;
    }

    printObjEvent(kindOutput,
                  level,
                  TOSTR_FMT(" [obj] Print {} ({} objects): {}", className, cntr, output));
}

}   // namespace tsv::debuglog
