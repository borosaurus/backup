#pragma once

#include "value.h"
#include "instructions.h"
#include "analysis.h"

enum InstrCode : char {
    kLoadConst,
    kAdd,
    kEq,
    kFillEmpty,
    kTestEq,
    kJmp,
};

struct ExecInstructions {
    void append(InstrLoadConst instr) {
        instructions.push_back(InstrCode::kLoadConst);
        instructions.push_back(instr.dst);

        // TODO: This should look up whether we already have the constant anywhere.
        constants.push_back(instr.constVal);
        
        auto* addr = allocateSpace(sizeof(uint16_t));
        const uint16_t offset16 = uint16_t(constants.size() - 1);
        writeToMemory<uint16_t>(addr, offset16);

        assert(instructions.size() % 4 == 0);
    }
    void append(InstrAdd instr) {
        instructions.push_back(InstrCode::kAdd);
        instructions.push_back(instr.dst);
        instructions.push_back(instr.left);
        instructions.push_back(instr.right);
        assert(instructions.size() % 4 == 0);
    }
    void append(InstrEq instr) {
        instructions.push_back(InstrCode::kEq);
        instructions.push_back(instr.dst);
        instructions.push_back(instr.left);
        instructions.push_back(instr.right);
    }
    void append(InstrFillEmpty instr) {
        instructions.push_back(InstrCode::kFillEmpty);
        instructions.push_back(instr.dst);
        instructions.push_back(instr.left);
        instructions.push_back(instr.right);
    }
    void append(InstrTestEq instr) {
        instructions.push_back(InstrCode::kTestEq);
        instructions.push_back(instr.left);
        instructions.push_back(instr.right);
        instructions.push_back(0); // padding
    }
    void append(InstrJmp instr) {
        instructions.push_back(InstrCode::kJmp);
        instructions.push_back(0); // padding
        auto* addr = allocateSpace(sizeof(uint16_t));
        writeToMemory<uint16_t>(addr, instr.off);

        assert(instructions.size() % 4 == 0);
    }
    
    
    char* allocateSpace(size_t size) {
        auto oldSize = instructions.size();
        instructions.resize(oldSize + size);
        return (char*)instructions.data() + oldSize;
    }
    
    std::vector<char> instructions;
    std::vector<ValTagOwned> constants;
};

struct AssembleCtx {
    CompilationResult* compilationResult;
    Register nextNewRegister = 1; // Doesn't handle overflow
    std::map<TempId, Register> tempToRegister;

    // Changes as we go through the instructions
    std::map<Register, TempId> currentRegisterMap;
};

bool isTempLive(AssembleCtx* ctx,
                TempId id,
                size_t instructionIdx) {
}

void chooseRegister(AssembleCtx* ctx,
                    TempId id,
                    size_t instructionIdx) {
    // (Not optimal)

    // Go through currently allocated registers, and determine if any of the temporaries
    // they represent are no longer alive.
    for (auto& [reg, tempId] : ctx->currentRegisterMap) {
        if (!isTempLive(ctx, tempId, instructionIdx)) {
        }
    }
    for (size_t i = instructionIdx; i < ctx->compilationResult->instructions.size(); ++i) {
        
    }
}
                          

ExecInstructions assemble(CompilationResult* r) {
    AssembleCtx ctx;
    // We always put the output in register 0.
    ctx.tempToRegister[r->tempId] = Register(0);

    // Generate mapping for registers.
    size_t idx = 0;
    for (const auto& instr : r->instructions) {
        std::visit(
            Overloaded{
                [&](LInstrLoadConst lc) {
                    if (ctx.tempToRegister.count(lc.dst)) {
                        // Already have a register for this temporary, do nothing.
                    } else {
                    }
                },
                [&](LInstrAdd a) {
                },
                [&](LInstrFillEmpty a) {
                },
                [&](LInstrJmp j) {
                },
                [&](LInstrMove m) {
                },
                [&](LInstrMovePhi m) {
                },
                [&](LInstrLabel l) {
                },
                [&](LInstrTestTruthy t) {
                },
                [&](LInstrTestFalsey t) {
                },
                [&](LInstrTestNothing t) {
                }
            },
            instr);

        ++idx;
    }

    ExecInstructions ret;
    return ret;
    
}
