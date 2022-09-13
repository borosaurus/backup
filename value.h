#pragma once

#include <iostream>
#include <variant>
#include <math.h>
#include <vector>
#include <optional>

template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
Overloaded(Ts...)->Overloaded<Ts...>;

template <typename T>
T readFromMemory(const char* ptr) noexcept {
    T val;
    memcpy(&val, ptr, sizeof(T));
    return val;
}

template <typename T>
size_t writeToMemory(char* ptr, const T val) noexcept {
    memcpy(ptr, &val, sizeof(T));
    return sizeof(T);
}

template<class T, class V>
std::optional<T> getAlternative(V&& v) {
    if (std::holds_alternative<T>(v)) {
        return std::get<T>(v);
    }
    return {};
}


using Value = uint64_t;
using Tag = uint8_t;

const Tag kTagNothing = 0;
const Tag kTagInt = 1;
const Tag kTagBool = 2;
const Tag kTagSlot = 3;





struct ValTagOwned {
    Value val;
    Tag tag;
    bool owned = false;

    bool operator==(const ValTagOwned&) const = default;
};
static_assert(std::is_trivially_copyable<ValTagOwned>::value);
static_assert(sizeof(ValTagOwned) == 16);

ValTagOwned makeInt(int v) {
    return ValTagOwned{Value(v), kTagInt, false};
}
ValTagOwned makeNothing() {
    return ValTagOwned{0, kTagNothing, false};
}
ValTagOwned makeBool(bool b) {
    return ValTagOwned{Value(b), kTagBool, false};
}

struct ValTag {
    Value val;
    Tag tag;
};
struct SlotAccessor {
    ValTagOwned data;
};
