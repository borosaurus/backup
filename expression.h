#pragma once

#include "value.h"
#include "instructions.h"

struct LogicalInstructions {
    std::vector<LInstr> instructions;
};

struct CompileCtx {
    TempId tempId = 0;

    TempId nextId() {
        return tempId++;
    }
};

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
                    }
                },
                instr);

            out += "\n";
        }
        return out;
    }
};


// Expression
struct Expression {
    virtual CompilationResult compile(CompileCtx*) = 0;
    virtual ~Expression() {
    }
};

struct ExpressionConst : public Expression {
    ExpressionConst(ValTagOwned c): constVal(c) {}

    virtual CompilationResult compile(CompileCtx* ctx) {
        auto id = ctx->nextId();
        return {id, {LInstrLoadConst{id, constVal}}};
    }

    ValTagOwned constVal;
};

struct ExpressionAdd : public Expression {
    ExpressionAdd(std::unique_ptr<Expression> l, std::unique_ptr<Expression> r):
        left(std::move(l)),
        right(std::move(r)){
    }

    virtual CompilationResult compile(CompileCtx* ctx) {
        auto id = ctx->nextId();

        CompilationResult res{id};
        auto leftRes = left->compile(ctx);
        auto rightRes = right->compile(ctx);

        res.append(leftRes.instructions);
        res.append(rightRes.instructions);

        res.instructions.push_back(LInstrAdd{id, leftRes.tempId, rightRes.tempId});

        return res;
    }

    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
};


///
