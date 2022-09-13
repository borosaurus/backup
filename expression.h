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
struct Expression;
using OwnedExpression = std::unique_ptr<Expression>;
struct Expression {
    virtual CompilationResult compile(CompileCtx*) = 0;
    virtual OwnedExpression optimize(OwnedExpression self) {
        return self;
    }
    // virtual std::string print() const = 0;
    virtual ~Expression() {
    }
};


struct ExpressionConst : public Expression {
    ExpressionConst(ValTagOwned c): constVal(c) {}

    virtual CompilationResult compile(CompileCtx* ctx) {
        auto id = ctx->nextId();
        return {id, {LInstrLoadConst{id, constVal}}};
    }
    // virtual std::string print() const {
    //     return "Const(" + std::to_string(constVal.tag) + ", " + std::to_string(constVal.val) + ")";
    // }

    ValTagOwned constVal;
};
std::unique_ptr<ExpressionConst> makeConstInt(int val) {
    return std::make_unique<ExpressionConst>(ValTagOwned{Value(val), kTagInt, false});
}

enum class BinOpType {
    kAdd,
    kAnd
};

struct ExpressionNOp : public Expression {
    ExpressionNOp(BinOpType t, std::vector<std::unique_ptr<Expression>> ins)
        :type(t),
         ins(std::move(ins))
    {}

    std::unique_ptr<Expression> optimize(OwnedExpression self) {
        if (type == BinOpType::kAnd) {
            std::vector<OwnedExpression> newIns;
            for (auto& c : ins) {
                if (auto* ptr = dynamic_cast<ExpressionNOp*>(c.get()); ptr && ptr->type == type) {
                    for (auto& node : ptr->ins) {
                        newIns.push_back(std::move(node));
                    }
                } else {
                    newIns.push_back(std::move(c));
                }
            }

            ins = std::move(newIns);
        }
        return self;
    }

    virtual CompilationResult compile(CompileCtx* ctx) {
        auto id = ctx->nextId();
        CompilationResult res{id};
        if (type == BinOpType::kAnd) {
            auto endLabel = ctx->nextLabel();

            std::vector<TempId> tempIds;
            for (size_t i = 0; i < ins.size() - 1; ++i) {
                auto exprRes = ins[i]->compile(ctx);
                res.append(exprRes.instructions);

                // If it's nothing, we jump to the end.
                res.instructions.push_back(LInstrTestNothing{exprRes.tempId});
                res.instructions.push_back(LInstrJmp{endLabel});

                // If it's falsey, we jump to the end.
                res.instructions.push_back(LInstrTestFalsey{exprRes.tempId});
                res.instructions.push_back(LInstrJmp{endLabel});

                // Otherwise we evaluate the next one.
                tempIds.push_back(exprRes.tempId);
            }

            // Evaluate the last one. For this, there's no need to jump.
            auto lastRes = ins.back()->compile(ctx);
            res.append(lastRes.instructions);
            tempIds.push_back(lastRes.tempId);

            res.instructions.push_back(LInstrLabel{endLabel});
            // Phi function
            res.instructions.push_back(LInstrMovePhi{id, tempIds});
            return res;
        } else {
            assert(0);
        }
    }

    BinOpType type;
    std::vector<std::unique_ptr<Expression>> ins;
};

struct ExpressionBinOp : public Expression {
    ExpressionBinOp(BinOpType t, std::unique_ptr<Expression> l, std::unique_ptr<Expression> r):
        type(t),
        left(std::move(l)),
        right(std::move(r)){
    }

    std::unique_ptr<Expression> optimize(OwnedExpression self) {
        left = left->optimize(std::move(left));
        right = right->optimize(std::move(right));

        if (type == BinOpType::kAnd) {
            std::vector<std::unique_ptr<Expression>> children;
            children.push_back(std::move(left));
            children.push_back(std::move(right));
            auto res = std::make_unique<ExpressionNOp>(type, std::move(children));
            return res->optimize(std::move(res));
        }
        return self;
    }

    virtual CompilationResult compile(CompileCtx* ctx) {
        auto id = ctx->nextId();
        CompilationResult res{id};

        auto leftRes = left->compile(ctx);
        auto rightRes = right->compile(ctx);
        
        if (type == BinOpType::kAnd) {
            assert(0); // We always use the n ary one for compilation
        } else if (type == BinOpType::kAdd) {
            res.append(leftRes.instructions);
            res.append(rightRes.instructions);

            res.instructions.push_back(LInstrAdd{id, leftRes.tempId, rightRes.tempId});
        } else {
            assert(0);
        }

        return res;
    }

    BinOpType type;
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
std::unique_ptr<ExpressionVariable> makeVariable(std::string s) {
    return std::make_unique<ExpressionVariable>(s);
}

struct LetBind {
    LetBind(std::string n, std::unique_ptr<Expression> e):name(n), expr(std::move(e)) {}
    
    std::string name;
    std::unique_ptr<Expression> expr;
};
    
struct ExpressionLet : public Expression {
    ExpressionLet(std::vector<LetBind> bs, std::unique_ptr<Expression> body) :binds(std::move(bs)), body(std::move(body)) {}

    std::unique_ptr<Expression> optimize(OwnedExpression self) {
        for (auto& b : binds) {
            b.expr = b.expr->optimize(std::move(b.expr));
        }
        body = body->optimize(std::move(body));
        return self;
    }
    
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

        
    std::unique_ptr<Expression> optimize(OwnedExpression self) {
        condition = condition->optimize(std::move(condition));
        then = then->optimize(std::move(then));
        els = els->optimize(std::move(els));
        return self;
    }

    virtual CompilationResult compile(CompileCtx* ctx) {
        auto id = ctx->nextId();
        
        CompilationResult res{id};
        auto condRes = condition->compile(ctx);

        res.append(condRes.instructions);

        auto trueLabel = ctx->nextLabel();
        auto endLabel = ctx->nextLabel();

        // If the condition evaluates to nothing, we jump past the whole thing.
        res.instructions.push_back(LInstrTestNothing{condRes.tempId});
        res.instructions.push_back(LInstrJmp{endLabel});

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
        res.instructions.push_back(LInstrMovePhi{id, {condRes.tempId, /* if its nothing */
                                                          elseRes.tempId,
                                                          thenRes.tempId}});

        return res;
    }
    
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Expression> then;
    std::unique_ptr<Expression> els;  
};

struct ExpressionCall : public Expression {
    ExpressionCall(std::string fnName, std::vector<OwnedExpression> args)
        :fnName(fnName),
         args(std::move(args))
    {}

    std::unique_ptr<Expression> optimize(OwnedExpression self) {
        for (auto& arg : args) {
            arg = arg->optimize(std::move(arg));
        }
        return self;
    }
    
    virtual CompilationResult compile(CompileCtx* ctx) {
        auto id = ctx->nextId();
        CompilationResult res{id};
        if (fnName == "fillEmpty") {
            assert(args.size() == 2);
            auto leftRes = args[0]->compile(ctx);
            auto rightRes = args[1]->compile(ctx);
            res.append(leftRes.instructions);
            res.append(rightRes.instructions);
            res.instructions.push_back(LInstrFillEmpty{id, leftRes.tempId, rightRes.tempId});
        } else {
            assert(0);
        }
        return res;
    }

    std::string fnName;
    std::vector<OwnedExpression> args;
};
std::unique_ptr<Expression> makeFillEmptyFalse(std::unique_ptr<Expression> e) {
    std::vector<OwnedExpression> args;
    args.push_back(std::move(e));
    args.push_back(std::make_unique<ExpressionConst>(makeBool(false)));
    return std::make_unique<ExpressionCall>("fillEmpty", std::move(args));
}



///
