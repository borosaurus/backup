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
struct LInstrMove {
    TempId dst;
    TempId src;
};
struct LInstrMovePhi {
    TempId dst;
    TempId srcA;
    TempId srcB;
};
struct LInstrAdd {
    TempId dst;
    TempId left;
    TempId right;
};
struct LInstrTestEq {
    TempId left;
    TempId right;
};
struct LInstrTestTruthy {
    TempId reg;
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
    LInstrLabel,
    LInstrTestTruthy,
    LInstrMove,
    LInstrMovePhi,
    LInstrJmp
    >;

struct CompilationResult {
    TempId tempId;
    std::vector<LInstr> instructions;

    void append(std::vector<LInstr>& moreInstructions) {
        instructions.insert(instructions.end(),
                            moreInstructions.begin(),
                            moreInstructions.end());
    }

    std::string tmpStr(TempId id) {
        return "T" + std::to_string(id);
    }

    std::string print() {
        std::string out;

        out += "Result at T" + std::to_string(tempId) + "\n";
        for (auto& instr: instructions) {
            std::visit(Overloaded{
                    [&](LInstrLoadConst lc) {
                        out += "loadc   " + tmpStr(lc.dst) + " " +
                            std::to_string(lc.constVal.tag) + ", " + std::to_string(lc.constVal.val);
                    },
                    [&](LInstrAdd a) {
                        out += "add     " + tmpStr(a.dst) + " " + tmpStr(a.left) + " " +
                            tmpStr(a.right);
                    },
                    [&](LInstrJmp j) {
                        out += "jmp     " + j.labelName;
                    },
                    [&](LInstrMove m) {
                        out += "mov     " + tmpStr(m.dst) + " " + tmpStr(m.src);
                    },
                    [&](LInstrMovePhi m) {
                        out += "mov     " + tmpStr(m.dst) + " phi(" +
                            tmpStr(m.srcA) + ", " + tmpStr(m.srcB) + ")";
                    },
                    [&](LInstrLabel l) {
                        out += l.name + ":";
                    },
                    [&](LInstrTestTruthy t) {
                        out += "testt   " + tmpStr(t.reg);
                    }
                },
                instr);

            out += "\n";
        }
        return out;
    }
};


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
