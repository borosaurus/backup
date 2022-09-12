#pragma once

#include <iostream>
#include <variant>
#include <math.h>
#include <vector>

using TempId = uint32_t;
struct LInstrLoadConst {
    TempId dst;
    ValTagOwned constVal;
};
struct LInstrAdd {
    TempId dstTmp;
    TempId leftTmp;
    TempId rightTmp;
};
struct LInstrTestEq {
    TempId left;
    TempId right;
};
struct LInstrJmp {
    std::string labelName;
};
struct LInstrLabel {
    std::string name;
};

using LInstr = std::variant<
    LInstrLoadConst,
    LInstrAdd,
    LInstrTestEq,
    LInstrJmp
    >;

// After register allocation
using Register = uint8_t;

struct InstrLoadConst {
    Register dst;
    ValTagOwned constVal;
};
struct InstrAdd {
    Register dst;
    Register left;
    Register right;
};
// Computes whether left and right are equal.
struct InstrEq {
    Register dst;
    Register left;
    Register right;
};
struct InstrFillEmpty {
    Register dst;
    Register left;
    Register right;
};

// Tests whether left and right are equal, skipping the following jump if not.
struct InstrTestEq {
    // No dest
    Register left;
    Register right;
};
struct InstrJmp {
    uint16_t off;
};

const size_t kInstructionSize = 4;

using Instr = std::variant<
    InstrLoadConst,
    InstrAdd,
    InstrEq,
    InstrFillEmpty,
    InstrTestEq,
    InstrJmp
    >;
