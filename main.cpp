#include <iostream>
#include <variant>
#include <math.h>
#include <vector>

#include "value.h"
#include "expression.h"
#include "instructions.h"
#include "optimize.h"
#include "exec.h"

void runFull(OwnedExpression expr) {
    assert(expr);
    std::cout << "RUNNING\n";
    CompileCtx ctx;
    expr = expr->optimize(std::move(expr));

    auto res = expr->compile(&ctx);

    std::cout << res.print() << std::endl;

    std::cout << "SSA pass\n";
    OptimizationCtx optCtx;
    optimizePreSSA(&optCtx, &res);
    std::cout << res.print() << std::endl;

    std::cout << "Removing phi...\n";
    removePhi(&res);
    std::cout << res.print() << std::endl;

    std::cout << "Post SSA optimizations...\n";
    optimizePostSSA(&optCtx, &res);
    std::cout << res.print() << std::endl;

    std::cout << "Assembling..\n";
    auto execInstructions = assemble(&res);

    std::cout << "Assembled to\n";
    std::cout << execInstructions.print() << std::endl;

    std::cout << "Running\n";
    Function fnInfo{execInstructions.numRegisters};
    Runtime rt(
        fnInfo,
        std::move(execInstructions.instructions),
        std::move(execInstructions.constants)
        );

    rt.run();
    std::cout << (int)rt.result().tag << " " << rt.result().val << std::endl;
    
    std::cout << std::endl << std::endl;
}

int main() {
    {
        std::vector<LetBind> binds;
        binds.push_back(LetBind("foo", std::make_unique<ExpressionConst>(makeInt(123))));
        binds.push_back(LetBind("bar", std::make_unique<ExpressionConst>(makeInt(456))));
        auto andExpr = std::make_unique<ExpressionBinOp>(
            BinOpType::kAnd,
            std::make_unique<ExpressionBinOp>(
                BinOpType::kAnd,
                makeFillEmptyFalse(makeVariable("foo")),
                makeFillEmptyFalse(makeConstInt(2))),
            makeConstInt(3));

        auto letExpr = std::make_unique<ExpressionLet>(
            std::move(binds),
            std::move(andExpr));
        
        runFull(std::move(letExpr));
    }

    {
        SlotAccessor s1{makeInt(101)};
        
        auto iff = std::make_unique<ExpressionIf>(
            std::make_unique<ExpressionVariable>("foo"),
            std::make_unique<ExpressionBinOp>(
                BinOpType::kAdd,
                std::make_unique<ExpressionVariable>("foo"),
                std::make_unique<ExpressionBinOp>(
                    BinOpType::kAdd,
                    makeSlot(&s1),
                    //makeConstInt(3),
                    makeConstInt(4)
                    )),
            makeConstInt(0)
            );
            
        std::vector<LetBind> binds;
        binds.push_back(LetBind("foo", std::make_unique<ExpressionConst>(makeInt(100))));
        
        auto letExpr = std::make_unique<ExpressionLet>(
            std::move(binds),
            std::move(iff)
            );

        runFull(std::move(letExpr));
    }



    ExecInstructions execInstructions;
    execInstructions.append(InstrLoadConst{Register(0), makeInt(123)});
    execInstructions.append(InstrLoadConst{Register(1), makeInt(123)});
    execInstructions.append(InstrLoadConst{Register(2), makeBool(false)});
    execInstructions.append(InstrTestEq{Register(0), Register(1)});
    execInstructions.append(InstrJmp{4});
    execInstructions.append(InstrAdd{Register(0), Register(0), Register(1)});
    execInstructions.append(InstrAdd{Register(0), Register(0), Register(0)});
    //execInstructions.append(InstrFillEmpty{Register(0), Register(0), Register(2)});

    Function fnInfo{3};
    Runtime rt(
        fnInfo,
        std::move(execInstructions.instructions),
        std::move(execInstructions.constants)
        );

    rt.run();
    std::cout << (int)rt.result().tag << " " << rt.result().val << std::endl;
    
    return 0;
}
