#pragma once

#include <iostream>
#include <variant>
#include <math.h>
#include <vector>

using TempId = uint32_t; // SSA temporary form

// After register allocation
using Register = uint8_t;

struct InstrLoadSlot {
    Register reg;
    uint8_t slotNum; // TODO: Could support larger size here
};
struct InstrLoadConst {
    Register dst;
    ValTagOwned constVal;
};
struct InstrAdd {
    // TempId dstTmp;
    // TempId leftTmp;
    // TempId rightTmp;
    
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
    InstrLoadSlot,
    InstrLoadConst,
    InstrAdd,
    InstrEq,
    InstrFillEmpty,
    InstrTestEq,
    InstrJmp
    >;
