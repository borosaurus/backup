#pragma once

#include "instructions.h"

bool isTempRead(CompilationResult* r, TempId id, size_t start = 0) {
    for (size_t i = start; i < r->instructions.size(); ++i) {
        const auto& instr = r->instructions[i];
        bool usedHere = std::visit(
            Overloaded{
                [&](LInstrLoadConst lc) {
                    return false;
                },
                [&](LInstrLoadSlot lc) {
                    return false;
                },
                [&](LInstrAdd a) {
                    return a.left == id || a.right == id;
                },
                [&](LInstrFillEmpty a) {
                    return a.left == id || a.right == id;
                },
                [&](LInstrJmp j) {
                    return false;
                },
                [&](LInstrMove m) {
                    return m.src == id;
                },
                [&](LInstrMovePhi m) {
                    return std::find(m.sources.begin(),
                                     m.sources.end(),
                                     id) != m.sources.end();
                },
                [&](LInstrLabel l) {
                    return false;
                },
                [&](LInstrTestTruthy t) {
                    return t.reg == id;
                },
                [&](LInstrTestFalsey t) {
                    return t.reg == id;
                },
                [&](LInstrTestNothing t) {
                    return t.reg == id;
                }
            },
            instr);
        if (usedHere) {
            return true;
        }
    }
    return false;
}
