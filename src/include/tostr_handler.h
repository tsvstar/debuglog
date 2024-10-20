#pragma once

/**
  Purpose: Universal tuneable conversion to string
            [ special cases implementations here ]
  Author: Taranenko Sergey (tsvstar@gmail.com)
  Date: 02-Aug-2017 ... 19-Oct-2024
  License: BSD. See License.txt
*/

#include <string>
#include <type_traits>      // SFINAE checks
#include <typeinfo>         // typeid

// Include enum reflection library
#if !defined(PREVENT_REFLECT_ENUM) && __has_include(<reflect>) && __cplusplus >= 202002L
   // Prioritized choice - qlibs/reflect as best compile-performance
   // temporary NTEST define is raised to improve compilation performance
#ifdef NTEST
#include <reflect>
#else
#define NTEST 1
#include <reflect>
#undef NTEST
#endif

#elif !defined(PREVENT_MAGIC_ENUM) && __has_include(<magic_enum.hpp>)
   // second choice - magic_enum as C++17 compatible
#include <magic_enum.hpp>
#define PREVENT_REFLECT_ENUM 1

#else
#define PREVENT_REFLECT_ENUM 1
#define PREVENT_MAGIC_ENUM 1

#endif

namespace tsv::util::tostr::settings
{
    extern bool showNullptrType;
    extern bool showUnknownValueAsRef;
    extern bool showFnPtrContent;
    extern bool showPtrContent;
    extern bool showEnumInteger;
}

// Forward declaration from debugresolve.h
namespace tsv::debuglog
{
    std::string demangle(const char* name);
    std::string resolveAddr2Name(const void* addr, bool addLineNum, bool includeHexAddr);
}

// Forward declarations
namespace tsv::debuglog::test_objlog
{
    class TrackedItem;
}
/* Do own classes for the "Your own classes special handlers" sections forward declarations here*/
/*  ..... */

// Forward declaration of STD.
// If no that classes used that prevents compilation error for that specialization
// If the class used, that means that corresponding header is included
namespace std
{
template<class T> class optional;
template<class T, class Deleter> class unique_ptr;
template<class T> class shared_ptr;
template<class T> class weak_ptr;
template<typename... Types> class variant;

template<typename Visitor, typename... Variants> constexpr decltype(auto) visit(Visitor&&, Variants&&...);
}

/********* MAIN PART **********/

namespace tsv::util::tostr
{

// Auxiliary function to decode pointer to hex string
std::string hex_addr( const void* ptr );

template<typename T>
std::string getTypeName()
{
    return ::tsv::debuglog::demangle(typeid(T).name());
}

/************************* KNOWN OBJECTS NAMES  *******************************/
// Feature to tag meaning of known objects to make log output more human-friendly
namespace known_pointers
{
    //function to print register/deregister event
    extern void (*known_ptr_logger_s)( const std::string& log_str );

    // return pointer's name if it is known
    std::string getName( const void* ptr );

    // If such ptr is known: true if pointer known AND have given typeName(ignore if empty), false otherwise
    bool isGenericPointerRegistered( const void* ptr, const std::string& typeName );

    // Register known pointer "ptr" with type "typeName" as name "ptrName"( empty ptrName - means "remove it" ).
    template<typename T>
    bool isPointerRegistered( const T* ptr )
    {
        return isGenericPointerRegistered( reinterpret_cast<const void*>( ptr ), getTypeName<T>() );
    }

    // Register known pointer "ptr" with type "typeName" as name "ptrName"(empty ptrName means "remove it" ).
    // Return previous value
    std::string registerPointerName( const void* ptr, const std::string& ptrName,
                                     const std::string& typeName, bool log = true );

    // Register known pointer "ptr" as name "ptrName" (empty ptrName means "remove it"" )
    template<typename T>
    std::string registerPointerName( const T* ptr, const std::string& ptrName, bool log = true )
    {
        return registerPointerName( reinterpret_cast<const void*>(ptr), ptrName, getTypeName<T>(), log );
    }

    // register known pointer "ptr" as name "ptrName" (if empty - remove it )
    template<typename T>
    std::string registerPointerNameWType( const T* ptr, const std::string& ptrName, bool log = true )
    {
        std::string typeName = getTypeName<T>();
        return registerPointerName( reinterpret_cast<const void*>( ptr ), ptrName + " (" + typeName + ")",
                                    typeName, log );
    }

    // Delete known pointer "ptr" with typename "typeName"(ignore typeName if empty)
    std::string deletePointerName( const void* ptr, const std::string& typeName, bool log = true );

    // delete known pointer "ptr"
    template<typename T>
    std::string deletePointerName( const T* ptr, bool log = true )
    {
        return deletePointerName( reinterpret_cast<const void*>( ptr ), getTypeName<T>(), log );
    }

}

/******************************************************
  toStr() internal processors implementations for types

  NOTE: processors are separated from main toStr() to
        make automatic dereferencing
*******************************************************/

// String conversion modes
enum ToStrEnum : int
{
    ENUM_TOSTR_DEFAULT,  // as string: value
    ENUM_TOSTR_REPR,     // as representation: "value"
    ENUM_TOSTR_EXTENDED  // extended info
};

namespace impl
{

// Return value of __toString
struct ToStringRV
{
    std::string value_;
    bool valid_;
};

// Default handler
template<typename T>
typename std::enable_if< !std::is_arithmetic<T>::value && !std::is_enum<T>::value, ToStringRV >::type
__toString(const T& /*value*/, int /*mode*/)
{
    // default case - generic kind of type
    return { "", false };   // no value
}

// Integral + floating_point handler
template<typename T>
typename std::enable_if< std::is_arithmetic<T>::value, ToStringRV >::type
__toString(const T& value, int /*mode*/)
{
    return { std::to_string( value ), true };
}

template<typename T>
typename std::enable_if< std::is_enum<T>::value, ToStringRV >::type
__toString(const T& value, int mode)
{
    std::string typeName = ::tsv::debuglog::demangle(typeid(T).name()) + "::";
#if !defined(PREVENT_REFLECT_ENUM)
   typeName += ::reflect::enum_name(value);
   if (tsv::util::tostr::settings::showEnumInteger)
       return  { typeName + "(" + std::to_string(value) + ")", true };
   return  { typeName, true };
#elif !defined(PREVENT_MAGIC_ENUM)
   typeName += ::magic_enum::enum_name(value);
   if (tsv::util::tostr::settings::showEnumInteger)
       return  { typeName + "(" + std::to_string(value) + ")", true };
   return  { typeName, true };
#else
    return { typeName + std::to_string(value), true };
#endif
}

// std::string handler
ToStringRV __toString(const std::string& value, int mode);
ToStringRV __toString(const std::string_view& value, int mode);
ToStringRV __toString(const bool value, int mode);

template<typename T>
ToStringRV __toString(const std::optional<T>& value, int mode)
{
   if (!value.has_value())
       return {"no_value", true};
   return __toString(*value, mode);
}

/**
       ... Your own classes special handlers  ...

To correct processing (and auto-dereferencing) your own classes forward declaration of handler have to be placed here
  and forward declaration of that class have to be placed in beginning of this file.
NOTE: Do not use specific to your class version of toStr(), otherwise pointers to it will not be auto-dereferenced

Example:

in beginning of this file:
    class YourClass;
here:
    ToStringRV toStr( const YourClass& value, int mode );

in .c:
   #include "tostr_handler.h"
   namespace tsv::util::tostr::impl {
        ToStringRV __toString(const ::YourClass& value, int mode)
        {
            // Return: { VALUE_STD::STRING, true }
            return { "value", true };
        }
   } } } }
**/

    // That is sample class from test_objlog
    ToStringRV __toString( const ::tsv::debuglog::test_objlog::TrackedItem& value, int mode );

}  // namespace impl



/**********************************************
    Main toStr() interface.

Purpose: Print val to std::string
Usage:
    std::string val = toStr(var);
    std::string val = toStr(var, ENUM_TOSTR_EXTENDED);
************************************************/
// Main implementation for values
template<typename T>
typename std::enable_if< !std::is_pointer<T>::value && !std::is_function<T>::value, std::string >::type
//typename std::enable_if<!std::is_pointer<T>::value && !std::is_base_of<C, T>::value, void>::type
toStr(const T& val, int mode = ENUM_TOSTR_DEFAULT)
{
    auto decoded = impl::__toString( val, mode );
    if ( !decoded.valid_ )
    {
        // If no known converter found, return typename (with reference if enabled)
        if ( !settings::showUnknownValueAsRef )
            return getTypeName<T>();

        // If that known shortcut, return it
        std::string pointerName = known_pointers::getName( &val );
        if ( !pointerName.empty() )
            return "*" + pointerName + "["+ hex_addr(&val)+"]";

        return "*" + hex_addr(&val) + " (" + getTypeName<T>() + ")";
    }

    return decoded.value_;
}

// Special case for function pointers
template<typename Ret, typename... Args>
std::string toStr(Ret (*val)(Args...), [[maybe_unused]]int mode = ENUM_TOSTR_DEFAULT)
{
    if ( !val )
    {
        if ( settings::showNullptrType )
            return "nullptr (" + getTypeName<Ret(*)(Args...)>() + ")";
        else
            return "nullptr";
    }

    std::string rv = hex_addr(reinterpret_cast<const void*>(val));
    if (settings::showFnPtrContent)
    {
        rv += " (" + getTypeName<Ret(*)(Args...)>() + "/"
             + tsv::debuglog::resolveAddr2Name(reinterpret_cast<const void*>(val),false,false)
             + ")";
    }

    return rv;
}

//TODO: special case for std::function<> and lambda(?) -- or detect it in const T& ...


// Main implementation for pointers
template<typename T>
std::string toStr(const T* val, int mode = ENUM_TOSTR_DEFAULT)
{
    if ( !val )
    {
        if ( settings::showNullptrType )
            return "nullptr (" + getTypeName<T>() + ")";
        else
            return "nullptr";
    }

    // if known shortcut, return it
    std::string rv = known_pointers::getName(reinterpret_cast<const void*>(val));
    if ( !rv.empty() )
        rv += "[" + hex_addr(reinterpret_cast<const void*>(val)) + "]";
    else
        rv = hex_addr(reinterpret_cast<const void*>(val));

    // If val is not pointer to pointer, then show its content
    // Important note: pointer should be valid
    if ( !std::is_pointer<T>() )
    {
        // Get content
        if (settings::showPtrContent)
        {
            auto decoded = impl::__toString( *val, mode );
            // If no known converter found, use typename as content
            if ( !decoded.valid_ )
               rv += " (" + getTypeName<T>() + ")";
            else
               rv += " (" + decoded.value_ + ")";
        }
     }

    return rv;
}

/**      Some basic specializations of handlers     **/
std::string toStr(void* v, int mode = ENUM_TOSTR_DEFAULT);
std::string toStr(std::nullptr_t value, int mode = ENUM_TOSTR_DEFAULT);
std::string toStr(const char* v, int mode = ENUM_TOSTR_DEFAULT);

/**
 * Extend impl with handlers which requires knowledge toStr
 */
namespace impl
{

//TODO: why it is not catched?
template<typename... Types>
ToStringRV __toString(const std::variant<Types...>& value, int mode)
{
    return std::visit([mode](const auto& val) -> ToStringRV {
            return tsv::util::tostr::toStr(val, mode);
         }, value);
}

template<typename T, class Deleter>
ToStringRV __toString(const std::unique_ptr<T, Deleter>& value, int mode)
{
    std::string rv = tsv::util::tostr::toStr( value.get(), mode );
    return {"unique_ptr:" + rv, true};
}

//TODO: why it is not catched?
template<typename T>
ToStringRV __toString(const std::shared_ptr<T>& value, int mode)
{
    std::string rv = tsv::util::tostr::toStr( value.get(), mode );
    return {"shared_ptr(" + std::to_string(value.use_count()) + "):" + rv, true};
}

//TODO: why it is not catched?
template<typename T>
ToStringRV __toString(const std::weak_ptr<T>& value, int mode)
{
    std::string rv = tsv::util::tostr::toStr( value.get(), mode );
    return {"weak_ptr(" + std::to_string(value.use_count()) + "):" + rv, true};
}


} // namespace impl

} // namespace tsv::util::tostr
