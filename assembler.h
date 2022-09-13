#pragma once

#include <sstream>

#include "value.h"
#include "instructions.h"
#include "analysis.h"

enum InstrCode : char {
    kLoadConst,
    kMove,
    kAdd,
    kEq,
    kFillEmpty,
    kTestEq,
    kTestTruthy,
    kTestFalsey,
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
    void append(InstrMove instr) {
        instructions.push_back(InstrCode::kMove);
        instructions.push_back(instr.dst);
        instructions.push_back(instr.src);
        instructions.push_back(88);
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
    void append(InstrTestTruthy instr) {
        instructions.push_back(InstrCode::kTestTruthy);
        instructions.push_back(instr.reg);
        instructions.push_back(99);
        instructions.push_back(99);
    }
    void append(InstrTestFalsey instr) {
        instructions.push_back(InstrCode::kTestFalsey);
        instructions.push_back(instr.reg);
        instructions.push_back(99);
        instructions.push_back(99);
    }
    
    size_t append(InstrJmp instr) {
        instructions.push_back(InstrCode::kJmp);
        instructions.push_back(0); // padding
        auto* addr = allocateSpace(sizeof(uint16_t));
        writeToMemory<uint16_t>(addr, instr.off);

        assert(instructions.size() % 4 == 0);

        return instructions.size() - sizeof(uint16_t);
    }
    
    
    char* allocateSpace(size_t size) {
        auto oldSize = instructions.size();
        instructions.resize(oldSize + size);
        return (char*)instructions.data() + oldSize;
    }

    size_t currentSize() const {
        return instructions.size();
    }

    std::string print() {
        std::stringstream out;
        
        const char* eip = instructions.data() - kInstructionSize;
        const char* end = instructions.data() + instructions.size();
        while (true) {
            out << "\n";
            eip += kInstructionSize;
            if (eip == end) {
                break;
            }
            assert(uintptr_t(eip - instructions.data()) % 4 == 0);
            assert(eip < end);
            InstrCode instrCode = *(InstrCode*)eip;
            out << (void*)eip << " ";

            switch(instrCode) {
            case kLoadConst: {
                auto regId = *(eip + 1);
                auto constId = readFromMemory<uint16_t>(eip + 2);
                
                out << "loadc       " << regStr(regId) << " " << "C(" <<
                    constants[constId].tag << ", " << constants[constId].val << ")";
                continue;
            }
            case kMove: {
                auto dstId = *(eip + 1);
                auto srcId = *(eip + 2);

                out << "mov         " << regStr(dstId) << " " << regStr(srcId);
                continue;
            }
            case kAdd: {
                auto dstReg = *(eip + 1);
                auto leftReg = *(eip + 2);
                auto rightReg = *(eip + 3);
                out << "add         " << regStr(dstReg) << " " <<
                    regStr(leftReg) << " " << regStr(rightReg);
                continue;
            }
            case kFillEmpty: {
                auto dstReg = *(eip + 1);
                auto valReg = *(eip + 2);
                auto rightReg = *(eip + 3);
                out << "fillempty   " << regStr(dstReg) << " " <<
                    regStr(valReg) << " " << regStr(rightReg);
                continue;
            }
            case kEq: {
                assert(0);
                break;
            }
            case kJmp: {
                auto off = readFromMemory<uint16_t>(eip + 2 /* skip padding */);
                // Add kInstructionSize, since we jump from the end of the jmp instruction.
                out << "jmp         " << (void*)(eip + off + kInstructionSize);
                continue;
            }
            case kTestEq: {
                auto l = *(eip + 1);
                auto r = *(eip + 2);
                out << "testeq      " << regStr(l) + " " << regStr(r);
                continue;
            }
            case kTestTruthy: {
                auto v = *(eip + 1);
                out << "testt       " << regStr(v);
                continue;
            }
            case kTestFalsey: {
                auto v = *(eip + 1);
                out << "testf       " << regStr(v);
                continue;
            }
            }

            // Should never get here, because of uses of continue in above loop.
            assert(0);
        }

        out << (void*)end << " <END>\n";
        return out.str();
    }
    
    std::vector<char> instructions;
    std::vector<ValTagOwned> constants;

    Register numRegisters = 0;
};

struct AssembleCtx {
    CompilationResult* compilationResult = nullptr;
    Register registerId = 1; // Doesn't handle overflow
    std::map<TempId, Register> tempToRegister;

    // Changes as we go through the instructions. Last element in the vector
    // is the current mapping.
    std::map<Register, std::vector<TempId>> registerToTemp;

    std::map<size_t, std::string> jumpsToFixUp;
    std::map<std::string, size_t> labelOffsets;

    Register nextRegister() {
        assert(registerId < 250);
        return registerId++;
    }

    Register regFor(TempId id) {
        assert(tempToRegister.count(id));
        return tempToRegister[id];
    }
};

bool isTempLive(AssembleCtx* ctx,
                TempId id,
                size_t instructionIdx) {
    // Assuming we only jump forward (which we do) this is right
    return isTempRead(ctx->compilationResult,
                      id,
                      instructionIdx);
}

void chooseRegister(AssembleCtx* ctx,
                    TempId id,
                    size_t instructionIdx) {
    std::cout << "Trying to choose register for " << tmpStr(id) << std::endl;
    if (ctx->tempToRegister.count(id)) {
        return;
    }
    // (Not optimal)

    // Go through currently allocated registers, and determine if any of the temporaries
    // they represent are no longer alive.
    for (auto& [reg, tempIds] : ctx->registerToTemp) {
        // We check if the temp is live AFTER this instruction, because during this instruction
        // the temp is still valid to use as a source. For example if I have
        // loadc T0, 123
        // add T1, T0, 234
        // <T0 never used again>
        // I can use the same register for T0 and T1.
        if (!isTempLive(ctx, tempIds.back(), instructionIdx + 1)) {
            std::cout << "reg " << reg << " for " << tmpStr(tempIds.back()) << " can be used for " <<
                tmpStr(id) << std::endl;
            ctx->tempToRegister[id] = reg;
            // Note that this invalidates iterators
            ctx->registerToTemp[reg].push_back(id);
            return;
        }
    }

    // Need a new register.
    auto reg = ctx->nextRegister();
    ctx->tempToRegister[id] = reg;
    ctx->registerToTemp[reg].push_back(id);
    std::cout << "Created new register " << regStr(reg) << " for " <<
        tmpStr(id) << std::endl;
}
                          

ExecInstructions assemble(CompilationResult* r) {
    AssembleCtx ctx{r};
    // We always put the output in register 0.
    ctx.tempToRegister[r->tempId] = Register(0);

    // Generate mapping for registers.
    size_t idx = 0;
    for (const auto& instr : r->instructions) {
        auto dest = getDest(instr);
        if (dest) {
            chooseRegister(&ctx,
                           *dest,
                           idx);
        }
        ++idx;
    }

    ExecInstructions ret;
    ret.numRegisters = ctx.registerId;
    
    for (auto& instr : r->instructions) {
        std::visit(
            Overloaded{
                [&](LInstrLoadConst lc) {
                    ret.append(
                        InstrLoadConst{
                            ctx.regFor(lc.dst),
                            lc.constVal
                        });
                },
                [&](LInstrAdd a) {
                    ret.append(InstrAdd{
                            ctx.regFor(a.dst),
                            ctx.regFor(a.left),
                            ctx.regFor(a.right)});
                },
                [&](LInstrFillEmpty a) {
                    ret.append(InstrFillEmpty{
                            ctx.regFor(a.dst),
                            ctx.regFor(a.left),
                            ctx.regFor(a.right)});
                },
                [&](LInstrJmp j) {
                    auto off = ret.append(InstrJmp{
                            999
                        });
                    ctx.jumpsToFixUp[off] = j.labelName;
                },
                [&](LInstrMove m) {
                    ret.append(InstrMove{
                            ctx.regFor(m.dst),
                            ctx.regFor(m.src)});
                },
                [&](LInstrMovePhi m) {
                    assert(0);
                },
                [&](LInstrLabel l) {
                    ctx.labelOffsets[l.name] = ret.currentSize();
                },
                [&](LInstrTestTruthy t) {
                    ret.append(InstrTestTruthy{ctx.regFor(t.reg)});
                },
                [&](LInstrTestFalsey t) {
                    ret.append(InstrTestFalsey{ctx.regFor(t.reg)});
                },
                [&](LInstrTestNothing t) {
                    assert(0);
                }
            },
            instr);
    }


    // Fix up the jumps.
    for (auto& [byteCodeOffset, label] : ctx.jumpsToFixUp) {
        // We add two to get the end of the jump instruction. That's where we jump from.
        auto diff = ctx.labelOffsets[label] - (byteCodeOffset + 2);
        writeToMemory<uint16_t>(ret.instructions.data() + byteCodeOffset,
                                diff);
    }
    
    return ret;
    
}
