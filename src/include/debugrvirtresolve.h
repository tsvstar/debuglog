#pragma once

/**
  Purpose: Resolving virt function call
  Author: Taranenko Sergey
  Date: 02-Aug-2017 ... 19-Oct-2024
  License: BSD. See License.txt

  NOTE1: If a lot virtual methods are defined for the class, 
         it could be needed to increase number in the VTIC_GET_INDEX macro
  NOTE2: It is better to include the header after "debuglog.h" to dynamically 
         excluding from compilation
*/

#if !defined(DEBUG_LOGGING) || DEBUG_LOGGING

#if !defined(__clang__) && defined(__GNUC__)
// Macro to get find out address of virtual function which will be actually called [GCC version]
// It uses GCC-specific extension "PMF convesrion" (Extracting the Function Pointer from a Bound Pointer to Member Function)
#pragma GCC diagnostic ignored "-Wpmf-conversions"
#define WHICH_VIRTFUNC_WILL_BE_CALLED( BaseClass, objPtr, method ) \
       ( objPtr ? reinterpret_cast<void*>(objPtr->*(&BaseClass::method)) : nullptr )
#endif



#if defined(__clang__) && __has_include(<boost/preprocessor/repeat.hpp>)

#include <boost/preprocessor/repeat.hpp>

namespace tsv::debuglog::resolve_helper
{

// The hacky way to determine index of function in vtable - we substitute instead of real class
// our own independend class. And when call with pointer-to-virtual-memeber on it,
// the vtable entry with exactly same index will be called (but that is our own class, so we
// control what are these new functions are - and they returns index)
template <class T, typename F>
int VTableIndex(F f)
{
    struct VTableCounter
    {
        #define VTIC_GET_INDEX(z, i, d) virtual int GetIndex_ ## i() { return i; }
        BOOST_PP_REPEAT(250, VTIC_GET_INDEX, unused);
        #undef VTIC_GET_INDEX
    } vt;

    T* t = reinterpret_cast<T*>(&vt);
    typedef int (T::*GetIndex)();
    GetIndex getIndex = reinterpret_cast<GetIndex>(f);
    return (t->*getIndex)();
}

// One more hacky step - after detecting index in vtable, extract corresponding entry from vtable
template <class B, typename F>
void* getVirtFuncPtr(const B* obj, F f)
{
    int idx = VTableIndex<B>(f);
    void** vtable = *(reinterpret_cast<void***>(const_cast<B*>(obj)));
    return vtable[idx];
}
}  // namespace tsv::debuglog::resolve_helper


// Macro to get find out address of virtual function which will be actually called (CLANG version)
#define WHICH_VIRTFUNC_WILL_BE_CALLED( BaseClass, objPtr, method ) \
        ( objPtr ? ::tsv::debuglog::resolve_helper::getVirtFuncPtr<BaseClass>( static_cast<const BaseClass*>(objPtr), &BaseClass::method) : nullptr)

#endif

#else

// If DEBUG_LOGGING is defined and forbid debugging, reduce to just nullptr
#define WHICH_VIRTFUNC_WILL_BE_CALLED( BaseClass, objPtr, method, ... ) nullptr
#endif

// If not defined due system requirements mismatch, fallback to error
#ifndef WHICH_VIRTFUNC_WILL_BE_CALLED
#define WHICH_VIRTFUNC_WILL_BE_CALLED( BaseClass, objPtr, method, ... ) static_assert(false, "No valuable WHICH_VIRTFUNC_WILL_BE_CALLED macro could be done for the compiler/lib")
#endif
