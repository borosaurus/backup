#pragma once

#include <map>

#include "value.h"
#include "instructions.h"

struct LogicalInstructions {
    std::vector<LInstr> instructions;
};

struct CompileCtx {
    TempId tempId = 0;
    int labelId = 0;

    TempId nextId() {
        return tempId++;
    }

    std::string nextLabel() {
        return "l" + std::to_string(labelId++);
    }

    std::map<std::string, TempId> varIds;
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
std::unique_ptr<ExpressionConst> makeConstInt(int val) {
    return std::make_unique<ExpressionConst>(ValTagOwned{Value(val), kTagInt, false});
}


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
    
struct ExpressionLet : public Expression {
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

struct ExpressionIf : public Expression {
    ExpressionIf(std::unique_ptr<Expression> c, std::unique_ptr<Expression> t,
                 std::unique_ptr<Expression> e)
        :condition(std::move(c)),
         then(std::move(t)),
         els(std::move(e))
    {}

    virtual CompilationResult compile(CompileCtx* ctx) {
        auto id = ctx->nextId();
        
        CompilationResult res{id};
        auto condRes = condition->compile(ctx);

        res.append(condRes.instructions);

        auto trueLabel = ctx->nextLabel();
        auto endLabel = ctx->nextLabel();

        // If the condition evaluates to nothing, we jump past the whole thing.

        res.instructions.push_back(LInstrTestTruthy{condRes.tempId});
        res.instructions.push_back(LInstrJmp{trueLabel});
        // Compile the false branch

        auto elseRes = els->compile(ctx);
        res.append(elseRes.instructions);
        //res.instructions.push_back(LInstrMove{id, elseRes.tempId});
        res.instructions.push_back(LInstrJmp{endLabel});
        
        res.instructions.push_back(LInstrLabel{trueLabel});
        auto thenRes = then->compile(ctx);
        res.append(thenRes.instructions);
        //res.instructions.push_back(LInstrMove{id, thenRes.tempId});
        res.instructions.push_back(LInstrLabel{endLabel});
        res.instructions.push_back(LInstrMovePhi{id, elseRes.tempId, thenRes.tempId});

        return res;
    }
    
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Expression> then;
    std::unique_ptr<Expression> els;  
};


///
