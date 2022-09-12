#pragma once

#include <map>

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

    std::map<std::string, TempId> varIds;
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

struct ExpressionVariable : public Expression {
    ExpressionVariable(std::string name): name(name) {
    }

    virtual CompilationResult compile(CompileCtx* ctx) {
        auto it = ctx->varIds.find(name);
        assert(it != ctx->varIds.end());
        CompilationResult res;
        res.tempId = it->second;
        return res;
    }

    std::string name;
};

struct LetBind {
    LetBind(std::string n, std::unique_ptr<Expression> e):name(n), expr(std::move(e)) {}
    
    std::string name;
    std::unique_ptr<Expression> expr;
};
    
struct ExpressionLet {
    ExpressionLet(std::vector<LetBind> bs, std::unique_ptr<Expression> body) :binds(std::move(bs)), body(std::move(body)) {}

    virtual CompilationResult compile(CompileCtx* ctx) {
        CompilationResult res;
        // Compile the bindings
        std::vector<TempId> bindTemps;
        for (auto& b : binds) {
            auto bindRes = b.expr->compile(ctx);
            res.append(bindRes.instructions);
            bindTemps.push_back(bindRes.tempId);
            ctx->varIds[b.name] = bindRes.tempId;
        }

        auto bodyRes = body->compile(ctx);
        res.append(bodyRes.instructions);
        res.tempId = bodyRes.tempId;

        for (auto& b : binds) {
            ctx->varIds.erase(b.name);
        }
        
        return res;
    }
    
    std::vector<LetBind> binds;
    std::unique_ptr<Expression> body;  
};

///
