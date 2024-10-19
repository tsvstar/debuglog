#pragma once

#include <stddef.h>
#include <type_traits>

/**
   Zero-overhead CPP properties (hidden getter/setter)   

   Usage example:
      struct Point { int Y; } 
    replaced to

      struct Point
      {
        int Y_;   //renamed value (which is underlying to Y)
        // define wrapper for Y_ which represent Y 
        def_prop (int, Point, Y,
            { return self->y_; },   // getter
            { self->y_ = value;}    // setter
        );
      };

        def_prop (int, Point, Y,
            { std::cout<<owner; return value_; },      // getter
            { std::cout<<owner; value_ = newValue;}    // setter
        );
*/

namespace properties_extension
{

template<
  typename Class,                  // Class which contain property [Owner]
  typename T,                      // type of property             [int]
  T const& (get)( Class const*, const char* comment ),   // static function getter
                                        // * Static member to avoid fragile member-pointers (and optimization of it)
  void (set)( Class*, T const&, const char* comment ),   // static function setter
                                        // * Static member to avoid fragile member-pointers (and optimization of it)
  size_t (offset) ()               // static function which returns offset of property in Owner
                                        // * We need to know offset of property to do not calculate this of Owner
                                        //   and in same time we don't know it before instantiation which is impossible
                                        //   with incomplete declaration of this template. So we use outer symbol
                                        // Usually it will be inlined.
>
struct prop
{
    prop() { set( self(), get( self(), nullptr ), "default ctor" ); }
    prop(const T& rhs) { set( self(), rhs, "value ctor" );}

    // Calculate "this" of Owner
    Class* self()
    {
        if (!offset)
            return nullptr;
        return reinterpret_cast<Class *> (reinterpret_cast<char *> (this)
                                         - offset ());
    }

    // Calculate "this" of Owner
    Class const* self() const
    {
        if (!offset)
            return nullptr;
        return reinterpret_cast<Class const *> (reinterpret_cast<char const *> (this)
                                               - offset ());
    }

    // Mutators
    // All kind of assignment operators ()
    prop& operator   = (T const& rhs) { set( self(), rhs, "op=" ); return *this; }
    prop& operator  += (T const& rhs) { set( self(), get( self(), "op+=" )  + rhs, "op+=" ); return *this; }
    prop& operator  -= (T const& rhs) { set( self(), get( self(), "op-=" )  - rhs, "op-=" ); return *this; }
    prop& operator  *= (T const& rhs) { set( self(), get( self(), "op*=" )  * rhs, "op*=" ); return *this; }
    prop& operator  /= (T const& rhs) { set( self(), get( self(), "op/=" )  / rhs, "op/=" ); return *this; }
    prop& operator  %= (T const& rhs) { set( self(), get( self(), "op%=" )  % rhs, "op%=" ); return *this; }
    prop& operator  ^= (T const& rhs) { set( self(), get( self(), "op^=" )  ^ rhs, "op^=" ); return *this; }
    prop& operator  |= (T const& rhs) { set( self(), get( self(), "op|=" )  | rhs, "op|=" ); return *this; }
    prop& operator  &= (T const& rhs) { set( self(), get( self(), "op&=" )  & rhs, "op&=" ); return *this; }
    prop& operator  <<= (T const& rhs) { set( self(), get( self(), "op<<=" ) << rhs, "op<<=" ); return *this; }
    prop& operator  >>= (T const& rhs) { set( self(), get( self(), "op>>=" ) >> rhs, "op>>=" ); return *this; }

    // Access via direct pointer is not trackeable so disable it
    T** operator&() = delete;

    // Accessors
    // operation of cast to T. That is enough for all kind of binary/unary operators
    //todo:
    // non-const are unsafe and should be rewritten - as we can't track possible changes
    // can't declare (event deleted) non-const because cause ambiguity
    operator T const& () const    { return get( self(), "cast" ); }

    //That is only if T is pointer
    template<typename U = T>
    std::enable_if_t<std::is_pointer_v<U>, U> operator->()
    {  return const_cast<U> ( get( self(), "op->" ) );  }

    template<typename U = T>
    std::enable_if_t<std::is_pointer_v<U>, U const> operator->()
    {   return get( self(), "op->" ); }

    // todo: don't need to have external member to refer?
    // require extending setter/getter prototype with T&
    T value_ {};

/*    T* operator ->()              {  return &const_cast<T&> ( get( self(), "op->" ) );  }
    T const* operator ->() const  {   return &get( self(), "op->" ); }*/
};

} // namespace properties_extension


#define def_prop( OwnerClass, MemberType, varName, getterBody, setterBody)                                        \
  static size_t prop_offset_ ## varName () { return offsetof (OwnerClass, varName); }                             \
  static MemberType const& prop_get_ ## varName (OwnerClass const* self, const char* comment ) getterBody         \
  static void prop_set_ ## varName (OwnerClass* self, MemberType const& value, const char* comment ) setterBody   \
  ::properties_extension::prop<OwnerClass, MemberType> varName {prop_get_ ## varName, prop_set_ ## varName, prop_offset_ ## varName};

namespace tsv::util::tostr::impl
{

template<
  typename Class,
  typename T,
  T const& (Getter)( Class const*, const char* comment ),
  void (Setter)( Class*, T const&, const char* comment ),
  size_t (Offset) () 
>
auto __toString( const ::properties_extension::prop<Class,T,Setter,Getter,Offset>& value, int mode)
{
    return __toString(Getter(value.self()), mode);
}

}

