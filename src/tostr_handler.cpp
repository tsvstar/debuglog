/**
  Purpose: Universal tuneable conversion to string
            [ special cases implementations here ]
  Author: Taranenko Sergey (tsvstar@gmail.com)
  Date: 02-Aug-2017 ... 19-Oct-2024
  License: BSD. See License.txt
*/

#include "tostr_fmt_include.h"
#include "tostr.h"

using namespace std;

#define STRINGIZE_DETAIL(x) #x
#define STRINGIZE(x) STRINGIZE_DETAIL(x)

namespace tsv::util::tostr
{

namespace settings
{

bool showNullptrType = true;
bool showUnknownValueAsRef = false; // for unknown by converter types show just "typeName"(0) or "*addr(typename)" (1)
bool showFnPtrContent = true;   // 1 - show type and resolved function name for the function pointer
bool showPtrContent = true;     // 1 - show in parenthesis dereferenced content of regular ptr
bool showEnumInteger = true;	// "ns::enum::valName" (0) or "ns::enum::valName(intValue)"(1)

}  // namespace tsv::util::tostr::settings

namespace
{
std::string_view findPrefix(std::string_view base)
{
    std::string_view::size_type pos = base.find("(");
    return base.substr(0,pos); 
}

bool startsWith(std::string_view base, std::string_view lookup )
{
    return ( base.size() >= lookup.size() ) && (base.substr(0,lookup.size()) == lookup );
}
} // anonymous namespace

bool Printer::detectNested(std::string_view base)
{
    static std::string prefixPrinter { findPrefix(STRINGIZE(TOSTR_JOIN())) };
    static std::string_view prefixFmt1 = "fmt::format(";
    static std::string_view prefixFmt2 = "std::format(";
    return (startsWith(base, prefixPrinter) || startsWith(base, prefixFmt1)
            || startsWith(base, prefixFmt2));
}


/*********** Special cases of toStr() *************/

// Special case: const char*
std::string toStr( const char* v, int mode )
{
    if ( !v )
        return "nullptr";
    if ( mode == ENUM_TOSTR_DEFAULT )
        return std::string(v);
    return "\"" + std::string(v) + "\"";
}

// Special case: void*
std::string toStr(void* v, int)
{
    return hex_addr(v);
}

// Special case: nullptr
std::string toStr(std::nullptr_t /*value*/, int /*mode*/)
{
    return "nullptr";
}

/*********** BEGIN OF namespace impl *************/
namespace impl
{

// Handler of std::string
ToStringRV __toString(const std::string& value, int mode)
{
    if ( mode == ENUM_TOSTR_DEFAULT )
        return { value, true };
    else
        return { "\"" + value + "\"", true };
}

ToStringRV __toString(const std::string_view& value, int mode)
{
    if ( mode == ENUM_TOSTR_DEFAULT )
        return { std::string(value), true };
    else
        return { "\"" + std::string(value) + "\"", true };
}

ToStringRV __toString(const bool value, int /*mode*/)
{
    return { value ? "true" : "false", true };
}

std::string __toStringULongHex(unsigned long value)
{
    return TOSTR_FMT("{:#x}", value);
}

std::string __toStringLongHex(long value)
{
    return TOSTR_FMT("{:#x}", value);
}

} // namespace impl



//Get pointer hex representation
std::string hex_addr(const void* ptr)
{
    if ( !ptr )
        return "nullptr";
    else
        return fmt::format("{}", ptr);
}
}   // 


/*********** BEGIN OF namespace known_pointers *************/

namespace tsv::util::tostr::known_pointers
{

struct TypeAndName
{
    std::string ptrName_;  
    std::string typeName_;
};

// If not nullptr, is used to logging register/delete known names events
void (*known_ptr_logger_s)( const std::string& log_str ) = nullptr;

// Container for known pointers names and their known types
auto& getKnownPtrMap()
{
    static std::unordered_map< const void*, TypeAndName > knownPtrMap( 100 );
    return knownPtrMap;
}

auto& getKnownPtrNames()
{
    static std::unordered_map< std::string, int > knownPtrNamesMap(100);
    return knownPtrNamesMap;
}

// Return pointer's name if it is known, empty otherwise
std::string getName( const void* ptr )
{
    auto& knownPtrMap_ = getKnownPtrMap();
    auto it = knownPtrMap_.find( ptr );
    if ( it == knownPtrMap_.end() )
        return "";
    return it->second.ptrName_;
}

// PURPOSE: Check if (typed) pointer known
// RETURN: true if pointer known AND have given typeName(ignore if empty), false otherwise
bool isGenericPointerRegistered( const void* ptr, const std::string& typeName )
{
    auto& knownPtrMap_ = getKnownPtrMap();
    auto it = knownPtrMap_.find( ptr );
    if ( it == knownPtrMap_.end() )
        return false;
    if ( !typeName.empty() && !it->second.typeName_.empty() && it->second.typeName_ != typeName )
        return false;
    return true;
}

// PURPOSE:      Register known pointer "ptr" with type "typeName" as name "ptrName" (if empty - remove it )
// RETURN VALUE: Previous known name of the "ptr'
std::string registerPointerName( const void* ptr, std::string ptrName,
                                 const std::string& typeName, bool log /*= true*/ )
{
    // Ignore nullptr
    if ( !ptr )
        return "nullptr";
    // Empty ptrName means delete
    if ( ptrName.empty() )
        return deletePointerName( ptr, typeName, true /*use-after-free-track*/ );

    auto& ptrNamesMap = getKnownPtrNames();
    auto it = ptrNamesMap.find( ptrName );
    if ( it == ptrNamesMap.end() )
        ptrNamesMap[ptrName] = 0;
    else
    {
        it->second++;
        ptrName += "$" + std::to_string(it->second);
    }

    auto& knownPtrMap_ = getKnownPtrMap();
    std::string prevPtrName = knownPtrMap_[ ptr ].ptrName_;
    knownPtrMap_[ ptr ] = { ptrName, typeName };
    if ( known_ptr_logger_s && log )
    {
        std::string logstr = std::string( prevPtrName.empty() ? "Register" : "Replace" ) + " knownPtr " +
                             hex_addr( ptr ) + " = " + ptrName + " // " + typeName;
        known_ptr_logger_s( logstr );
    }
    return prevPtrName;
}

// PURPOSE:      Delete known pointer "ptr" with typename "typeName"(ignore typeName if empty )
// RETURN VALUE: Known name of the "ptr" or empty if not found
std::string deletePointerName( const void* ptr, const std::string& typeName, bool useAfterFreeTrack, bool log /*= true*/ )
{
    auto& knownPtrMap_ = getKnownPtrMap();
    auto it = knownPtrMap_.find( ptr );
    if ( it == knownPtrMap_.end() )
        return "";
    if ( !typeName.empty() && !it->second.typeName_.empty() && it->second.typeName_ != typeName )
        return "";
    std::string prevPtrName = it->second.ptrName_;
    if ( known_ptr_logger_s && log )
    {
        std::string logstr = "Delete knownPtr " + hex_addr( ptr ) + ":  " + prevPtrName + " // " + it->second.typeName_;
        known_ptr_logger_s( logstr );
    }

    if (useAfterFreeTrack)
        // Do not really delete - just mark it for use-after-free catch
        it->second.ptrName_ += "(removed)";
    else
        knownPtrMap_.erase(it);

    return prevPtrName;
}

} // namespace tsv::util::tostr::known_pointers

