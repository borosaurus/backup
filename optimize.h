#pragma once

#include "instructions.h"
#include "analysis.h"

struct TempConstraints {
    bool canBeNothing = false;

    TempConstraints accumulateOr(TempConstraints t) {
        return TempConstraints{canBeNothing || t.canBeNothing};
    }
};

struct OptimizationCtx {
    std::map<TempId, TempConstraints> constraints;
};

size_t findDefinition(CompilationResult* r, TempId id) {
    size_t off = 0;
    for (auto& instr : r->instructions) {
        auto dest = getDest(instr);
        bool isHere = dest && *dest == id;
        
        if (isHere) {
            return off;
        }
        ++off;
    }

    return -1;
}



void replaceTemp(CompilationResult* r, TempId oldTemp, TempId newTemp) {
    for (auto& instr : r->instructions) {
        std::visit(
            Overloaded{
                [&](LInstrLoadConst& lc) {
                    if (lc.dst == oldTemp) {
                        lc.dst = newTemp;
                    }
                },
                [&](LInstrAdd& a) {
                    if (a.dst == oldTemp) {
                        a.dst = newTemp;
                    }
                    if (a.left == oldTemp) {
                        a.left = newTemp;
                    }
                    if (a.right == oldTemp) {
                        a.right = newTemp;
                    }
                },
                [&](LInstrFillEmpty& a) {
                    if (a.dst == oldTemp) {
                        a.dst = newTemp;
                    }
                    if (a.left == oldTemp) {
                        a.left = newTemp;
                    }
                    if (a.right == oldTemp) {
                        a.right = newTemp;
                    }
                },
                [&](LInstrJmp& j) {
                },
                [&](LInstrMove& m) {
                    if (m.dst == oldTemp) {
                        m.dst = newTemp;
                    }
                    if (m.src == oldTemp) {
                        m.src = newTemp;
                    }
                },
                [&](LInstrMovePhi& m) {
                    assert(0);
                },
                [&](LInstrLabel& l) {
                },
                [&](LInstrTestTruthy& t) {
                    if (t.reg == oldTemp) {
                        t.reg = newTemp;
                    }
                },
                [&](LInstrTestFalsey& t) {
                    if (t.reg == oldTemp) {
                        t.reg = newTemp;
                    }
                },
                [&](LInstrTestNothing& t) {
                    if (t.reg == oldTemp) {
                        t.reg = newTemp;
                    }
                }
            },
            instr);
    }
}

void removePhi(CompilationResult* r) {
    auto it = r->instructions.begin();

    while (it != r->instructions.end()) {
        auto instr = *it;
        if (std::holds_alternative<LInstrMovePhi>(instr)) {
            // Remove the phi instruction.
            r->instructions.erase(it);

            auto phiInstr = std::get<LInstrMovePhi>(instr);

            for (auto src : phiInstr.sources) {
                auto srcOffset = findDefinition(r, src);
                r->instructions.insert(r->instructions.begin() + srcOffset + 1,
                                       LInstrMove{phiInstr.dst, src});

                // auto srcBOffset = findDefinition(r, phiInstr.srcB);
                // r->instructions.insert(r->instructions.begin() + srcBOffset + 1,
                //                        LInstrMove{phiInstr.dst, phiInstr.srcB});
            }

            // Iterator is invalidated now, just go to the beginning. Not efficient,
            // but who cares?
            it = r->instructions.begin();
        } else {
            ++it;
        }
    }
}

void computeConstraints(OptimizationCtx* ctx, const CompilationResult* r) {
    for (const auto& instr : r->instructions) {
        std::visit(
            Overloaded{
                [&](LInstrLoadConst lc) {
                    ctx->constraints[lc.dst] = TempConstraints{lc.constVal.tag == kTagNothing};
                },
                [&](LInstrAdd a) {
                    // Could be smarter about this.
                    ctx->constraints[a.dst] = TempConstraints{true};
                },
                [&](LInstrFillEmpty a) {
                    ctx->constraints[a.dst] = TempConstraints{
                        // If you do fillEmpty(a, Nothing), then you still get nothing.
                        ctx->constraints[a.right].canBeNothing
                    };
                },
                [&](LInstrJmp j) {
                },
                [&](LInstrMove m) {
                    assert(ctx->constraints.count(m.src));
                    ctx->constraints[m.dst] = ctx->constraints[m.src];
                },
                [&](LInstrMovePhi m) {
                    TempConstraints constraint;
                    for (auto& src : m.sources) {
                        assert(ctx->constraints.count(src));
                        constraint = constraint.accumulateOr(ctx->constraints[src]);
                    }
                    ctx->constraints[m.dst] = constraint;
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
    }
}

struct OptimizationPass {
    virtual bool run(OptimizationCtx* ctx, CompilationResult* r) = 0;
    virtual ~OptimizationPass() = default;
};

struct DeadStorePass : public OptimizationPass {
    bool run(OptimizationCtx* ctx, CompilationResult* r) {
        bool didAnything = false;
        auto it = r->instructions.begin();
        while (it != r->instructions.end()) {
            auto dest = getDest(*it);
            // The temp representing the whole output is an exception.
            if (dest && *dest != r->tempId && !isTempRead(r, *dest)) {
                it = r->instructions.erase(it);
                didAnything = true;
            } else {
                ++it;
            }
        }
        return didAnything;
    }    
};

struct RemoveRedundantNothingPass : public OptimizationPass {
    bool run(OptimizationCtx* ctx, CompilationResult* r) {
        bool didAnything = false;
        auto it = r->instructions.begin();
        while (it != r->instructions.end()) {
            auto instr = *it;
            if (auto testNothing = getAlternative<LInstrTestNothing>(instr)) {
                if (!ctx->constraints[testNothing->reg].canBeNothing) {
                    // Erase this instruction and the following jump.
                    it = r->instructions.erase(it);
                    it = r->instructions.erase(it);
                    continue;
                }
            }

            if (auto fillEmpty = getAlternative<LInstrFillEmpty>(instr)) {
                if (!ctx->constraints[fillEmpty->left].canBeNothing) {
                    // Replace the fill empty with a simple move.
                    *it = LInstrMove{fillEmpty->dst, fillEmpty->left};
                    continue;
                }
            }
            ++it;
        }
        return didAnything;
    }
};

void optimizePreSSA(OptimizationCtx* ctx, CompilationResult* r) {
    computeConstraints(ctx, r);
    std::cout << "Constraints generated:\n";
    for (auto& [tempId, con] : ctx->constraints) {
        std::cout << tmpStr(tempId) << " " <<
            (con.canBeNothing ? "maybe-nothing" : "not-nothing") << std::endl;
    }
    
    std::vector<std::unique_ptr<OptimizationPass>> passes;
    passes.push_back(std::make_unique<RemoveRedundantNothingPass>());
    passes.push_back(std::make_unique<DeadStorePass>());
    for (auto& p : passes) {
        p->run(ctx, r);
    }
    // find move TA, TB instruction. If the definition of TB is available in this block then get rid
    // of the move and use TA everywhere TB is used.
}

struct BasicCopyPropPass : public OptimizationPass {
    bool run(OptimizationCtx* ctx, CompilationResult* r) {
        bool didAnything = false;
        
        auto it = r->instructions.begin();
        while (it != r->instructions.end()) {
            auto instr = *it;
        
            if (std::holds_alternative<LInstrMove>(instr)) {
                auto moveInstr = std::get<LInstrMove>(instr);
                auto srcDefinition = findDefinition(r, moveInstr.src);

                // Is there a jump between the definition and here?
                auto it2 = r->instructions.begin() + srcDefinition;

                bool foundJump = false;
                while (it2 != it) {
                    if (std::holds_alternative<LInstrJmp>(*it2)) {
                        foundJump = true;
                        break;
                    }
                    ++it2;
                }

                if (!foundJump) {
                    replaceTemp(r, moveInstr.src, moveInstr.dst);
                    it = r->instructions.erase(it);
                    didAnything = true;
                } else {
                    ++it;
                }
            } else {
                ++it;
            }
        }

        return didAnything;
    }
};

void optimizePostSSA(OptimizationCtx* ctx, CompilationResult* r) {
    std::vector<std::unique_ptr<OptimizationPass>> passes;
    passes.push_back(std::make_unique<BasicCopyPropPass>());

    for (auto& p : passes) {
        p->run(ctx, r);
    }
    // find move TA, TB instruction. If the definition of TB is available in this block then get rid
    // of the move and use TA everywhere TB is used.
}
