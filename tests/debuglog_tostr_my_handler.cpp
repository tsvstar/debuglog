// NOTE: "debuglog_tostr_my_handler.h" is automatically included
#include "tostr.h"

#include "test_tostr.h"	// for tsv::debuglog::tests::KnownClass


// Example definition of user-class tostr handler
namespace tsv::util::tostr::impl
{
ToStringRV __toString(const tsv::debuglog::tests::KnownClass& value, int mode)
{
     return { mode!=ENUM_TOSTR_EXTENDED 
               ? "BaseAppearance#"+std::to_string(value.x)
               :  "ExtAppearance#"+std::to_string(value.x)+"/"+std::to_string(value.y),
              true /*type match found*/};
}
}
