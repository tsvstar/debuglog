#pragma once
#include <variant>

namespace tsv::util::tostr
{
template<typename T> std::string toStr(const T&, int mode = 0);

namespace impl
{

template<typename... Types>
ToStringRV __toString(const std::variant<Types...>& value, int mode)
{
    return std::visit([mode](const auto& val) -> ToStringRV {
        return { tsv::util::tostr::toStr(val, mode), true };
    }, value);
}

}  // namespace impl
}  // namespace tsv::util::tostr
