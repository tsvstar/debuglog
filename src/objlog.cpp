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
bool ObjLogger::includeContextName_s = true;

namespace
{
    LastSentryLogger lastLogger("");


    void printObjEvent(SentryLogger::Kind kind, SentryLogger::Level level, std::string_view content)
    {
        auto* last = SentryLogger::getLast();
        last->write(kind, level, ' ', ObjLogger::includeContextName_s ? last->getContextName() : "", "", content, {} );
    }

    void printStackTrace(int depth, SentryLogger::Level level, SentryLogger::Kind kind)
    {
        if (depth)
            lastLogger.printStackTrace({/*.depth=*/depth, /*.skip=*/2, /*enforce*/false, /*.kind=*/kind}, level);
    }

    inline ptrdiff_t getOffs(const void* loggerSelf, const void* ptr, SentryLogger::Level level, SentryLogger::Kind kind)
    {
        if (!ptr || level > Settings::getLogLevel() || !Settings::isKindAllowed(kind))
            return -1;
        return (reinterpret_cast<const char*>(loggerSelf) - reinterpret_cast<const char*>(ptr));
    }

}   // namespace anonymous

    // -TODO: not sure that signed offs_ is safe. - it is. a) pointer ariphm only defined for the same block of memory and normal usage of objlogger assume that ptr>this

ObjLogger::ObjLogger( void* ptr, const char* className, int depth /*=0*/, const char* comment /*=""*/, SentryLogger::Level level /*=Default*/, SentryLogger::Kind kind /*=Tracked*/)
    : depth_( depth ), offs_( getOffs(this, ptr, level, kind) ), className_(className), kind_(kind), level_(level)
{
    // Untrackable object (due to level, kind, given ptr)
    if (offs_ < 0)
        return;

    getInstance().allObjectsMap[className_][ptr]++;
    getInstance().objectsCounter[className_]++;
    printObjEvent( kind_, level_, TOSTR_FMT( "[obj:{}:{}] create 0x{:x} {}", className, getInstance().objectsCounter[className], ptr, comment?comment:""));
    printStackTrace(depth_, level_, kind_);
}

ObjLogger::ObjLogger( void* ptr, void* copied_from, const char* className, int depth, const char* comment, SentryLogger::Level level /*=Default*/, SentryLogger::Kind kind /*=Tracked*/)
    : depth_( depth ), offs_( getOffs(this, ptr, level, kind) ), className_(className), kind_(kind), level_(level)
{
    // Untrackable object (due to level, kind, given ptr)
    if (offs_ < 0)
        return;
    getInstance().allObjectsMap[className_][ptr]++;
    getInstance().objectsCounter[className_]++;
    printObjEvent(kind_, level_, TOSTR_FMT( "[obj:{}:{}] {:x} copy_ctor(0x{:x}) {}", className, getInstance().objectsCounter[className_], ptr, copied_from, comment?comment:""));
    printStackTrace(depth_, level_, kind_);
}

ObjLogger::ObjLogger(const ObjLogger& obj) :
        depth_( obj.depth_ ), offs_( obj.offs_ ), className_( obj.className_ ), kind_(obj.kind_), level_(obj.level_)
{
    if ( offs_ > 0 )
    {
        const void* ptr = reinterpret_cast<const char*>(this) - offs_;
        const void* copied_from = reinterpret_cast<const char*>(&obj) - offs_;

        getInstance().allObjectsMap[className_][ptr]++;
        getInstance().objectsCounter[className_]++;
        printObjEvent(kind_, level_, TOSTR_FMT( "[obj:{}:{}] 0x{:x} copy_ctor_dflt(0x{:x})", className_, getInstance().objectsCounter[className_], ptr, copied_from));
        printStackTrace(depth_, level_, kind_);
    }
}

ObjLogger::ObjLogger( const ObjLogger&& obj ) :
        depth_( obj.depth_ ), offs_( obj.offs_ ), className_( obj.className_ ), kind_(obj.kind_), level_(obj.level_)
{
    if ( offs_ > 0 )
    {
        const void* ptr = reinterpret_cast<const char*>(this) - offs_;
        const void* copied_from = reinterpret_cast<const char*>(&obj) - offs_;

        getInstance().allObjectsMap[className_][ptr]++;
        getInstance().objectsCounter[className_]++;
        printObjEvent(kind_, level_, TOSTR_FMT("[obj:{}:{}] 0x{:x} copy_ctor_move(0x{:x})", className_, getInstance().objectsCounter[className_], ptr, copied_from));
        printObjEvent(kind_, level_, TOSTR_FMT("[obj:{}:{}] 0x{:x} become unitialized", className_, getInstance().objectsCounter[className_], copied_from));
        printStackTrace(depth_, level_, kind_);
    }
}


ObjLogger& ObjLogger::operator=(const ObjLogger& obj)
{
    if ( const_cast<const ObjLogger*>(this) == &obj )
        return *this;

    const void* ptr = ( offs_ > 0 ) ? reinterpret_cast<const char*>(this) - offs_ : nullptr;
    const void* copied_from = ( offs_ > 0 ) ? reinterpret_cast<const char*>(&obj) - obj.offs_ : nullptr;
    if ( offs_!= obj.offs_ || className_ != obj.className_ )
    {
      printObjEvent( kind_, level_, TOSTR_FMT("[obj] INCONSISTENCE operator=() -> className=%s|%s offs=%d|%d", className_, obj.className_, offs_, obj.offs_));
    }
    else
    {
      printObjEvent( kind_, level_, TOSTR_FMT("[obj:{}:{}] 0x{:x} operator=(0x{:x})", className_, getInstance().objectsCounter[className_], ptr, copied_from));
      // depth could be different
      depth_ = obj.depth_;
    }

    printStackTrace(depth_, level_, kind_);

    // Do nothing real work because both sides are exists (and so registered)

    return *this;
}



ObjLogger::~ObjLogger()
{
    if ( offs_ > 0 )
    {
        void* objPtr = reinterpret_cast<char*>( this ) - offs_ ;
        UnordMapPtrInt_t& obj = getInstance().allObjectsMap[ className_ ];
        UnordMapPtrInt_t::iterator objCntr = obj.find( objPtr );
        int& classCntr = getInstance().objectsCounter[className_];

        if ( objCntr == obj.end() )
            printObjEvent( kind_, level_, TOSTR_FMT("[obj:{}:{}] destroy 0x{:x}. ERROR: not registered pointer", className_, classCntr, objPtr));
        else
        {
            classCntr--;
            printObjEvent( kind_, level_, TOSTR_FMT("[obj:{}:{}] destroy 0x{:x}", className_, classCntr, objPtr));
            if ( objCntr->second > 1 )
                objCntr->second--;
            else
                obj.erase( objCntr );
        }

        printStackTrace(depth_, level_, kind_);
    }
}

// Print all or exact class pointers
//      className    = if nullptr print all tracked pointers
//                     otherwise only tracked pointers of given class
void ObjLogger::printTrackedPtr( SentryLogger::Level level, const char* className /*= nullptr */, SentryLogger::Kind kindOutput /* = SentryLogger:Kind::Tracked*/)
{
    //todo: add arg "filterByKind". if ==Parent then no filtering, otherwise filter by sentry.kind_ ??
    auto& allObjectsMap = getInstance().allObjectsMap;
    if ( !className )
    {
        printObjEvent( kindOutput, level, TOSTR_FMT(" [obj] Print all active pointers"));
        for ( auto& tmp : allObjectsMap )
            printTrackedPtr( level, tmp.first.c_str(), kindOutput );
        return;
    }

    auto it = allObjectsMap.find( className );
    if ( it == allObjectsMap.end() )
    {
        printObjEvent( kindOutput, level, TOSTR_FMT(" [obj] Print {}: not found such class", className));
        return;
    }

    std::string output;
    int cntr = 0;
    auto& ptrMap = it->second;
    for ( const auto& ptr : ptrMap )
    {
        if ( !ptr.second )
          continue;
        if ( ptr.second==1 )
            output.append(TOSTR_FMT("{}{:p}", cntr?",":"", ptr.first));
        else
            output.append(TOSTR_FMT("{}{:p}({})", cntr?",":"", ptr.first, ptr.second));
        cntr++;
    }

    printObjEvent( kindOutput, level, TOSTR_FMT(" [obj] Print {} ({} objects): {}", className, cntr, output));
}


}   // namespace tsv::debuglog
