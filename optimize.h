#pragma once

#include "instructions.h"

struct OptimizationContext {
};


size_t findDefinition(CompilationResult* r, TempId id) {
    size_t off = 0;
    for (auto& instr : r->instructions) {
        bool isHere = std::visit(
            Overloaded{
                [&](LInstrLoadConst lc) {
                    return lc.dst == id;
                },
                [&](LInstrAdd a) {
                    return a.dst == id;
                },
                [&](LInstrJmp j) {
                    return false;
                },
                [&](LInstrMove m) {
                    return m.dst == id;
                },
                [&](LInstrMovePhi m) {
                    return m.dst == id;
                },
                [&](LInstrLabel l) {
                    return false;
                },
                [&](LInstrTestTruthy t) {
                    return false;
                }
            },
            instr);
        
        if (isHere) {
            return off;
        }
        ++off;
    }

    return -1;
}

void removePhi(CompilationResult* r) {
    auto it = r->instructions.begin();

    while (it != r->instructions.end()) {
        auto instr = *it;
        
        if (std::holds_alternative<LInstrMovePhi>(instr)) {
            // Remove the phi instruction.
            it = r->instructions.erase(it);

            auto phiInstr = std::get<LInstrMovePhi>(instr);

            auto srcAOffset = findDefinition(r, phiInstr.srcA);
            r->instructions.insert(r->instructions.begin() + srcAOffset + 1,
                                   LInstrMove{phiInstr.dst, phiInstr.srcA});

            auto srcBOffset = findDefinition(r, phiInstr.srcB);
            r->instructions.insert(r->instructions.begin() + srcBOffset + 1,
                                   LInstrMove{phiInstr.dst, phiInstr.srcB});
        } else {
            ++it;
        }
    }
}

struct OptimizationPass {
    virtual bool run(OptimizationContext* ctx, CompilationResult* r) = 0;
};

struct BasicCopyPropPass : public OptimizationPass {
    
    
    bool run(OptimizationContext* ctx, CompilationResult* r) {
        auto it = r->instructions.begin();
        while (it != r->instructions.end()) {
            auto instr = *it;
        
            if (std::holds_alternative<LInstrMove>(instr)) {
                auto moveInstr = std::get<LInstrMove>(instr);
                
                bool canApply = false;
                // See if we can find the source definition near here.
                for (auto rit = std::make_reverse_iterator(it);
                     rit != r->instructions.rend();
                     ++rit) {

                    // If we see a jump or something, we have to give up.
                    bool giveUp = std::visit(
                        Overloaded{
                            [&](LInstrLoadConst lc) {
                                return false;
                            },
                            [&](LInstrAdd a) {
                                return false;
                            },
                            [&](LInstrJmp j) {
                                return true;
                            },
                            [&](LInstrMove m) {
                                return false;
                            },
                            [&](LInstrMovePhi m) {
                                // Assumes this is run after phi elimination
                                assert(0);
                            },
                            [&](LInstrLabel l) {
                                return false;
                            },
                            [&](LInstrTestTruthy t) {
                                return false;
                            }
                        },
                        instr);

                    if (giveUp) {
                        break;
                    }
                }

                if (canApply) {
                    //...
                }
            }

            ++it;
        }

        return false;
    }
};

void optimizePostSSA(CompilationResult* r) {
    // find move TA, TB instruction. If the definition of TB is available in this block then get rid
    // of the move and use TA everywhere TB is used.
}
