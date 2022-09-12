#pragma once

#include "instructions.h"

struct LogicalInstructions {
    std::vector<Instr> instructions;
};

struct CompileCtx {
    TempId tempId = 0;

    TempId nextId() {
        return tempId++;
    }
};


// Expression
struct Expression {
    virtual TempId compile(LogicalInstructions*) = 0;
};

struct ExpressionConst {
    virtual TempId compile(CompileCtx* ctx, LogicalInstructions* out) {
        auto id = ctx->nextId();
        //out->push_back(InstrLoadConst{id, ...});
        return id;
    }
};


///
