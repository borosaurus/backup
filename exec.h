#pragma once

#include "value.h"
#include "assembler.h"

struct Function {
    // Total number of local variables.
    int maxStackSize = 0;
};

struct Runtime {
    Runtime(Function fnInfo,
            std::vector<char> is,
            std::vector<ValTagOwned> st):
        currentFunction(fnInfo),
        instructions(std::move(is)),
        stack(std::move(st)) {
        base = st.size();

        assert(instructions.size() % 4 == 0);
    }

    void run() {
        // TODO: Should maybe put 'base' in a local variable? Check asm output.
        stack.resize(base + currentFunction.maxStackSize);

        const char* eip = instructions.data() - kInstructionSize;
        const char* end = instructions.data() + instructions.size();
        auto* stackBase = &stack[base];
        while (true) {
            eip += kInstructionSize;
            if (eip == end) {
                break;
            }
            assert(uintptr_t(eip - instructions.data()) % 4 == 0);
            assert(eip < end);
            InstrCode instrCode = *(InstrCode*)eip;
            switch(instrCode) {
            case kLoadConst: {
                auto regId = *(eip + 1);
                auto constId = readFromMemory<uint16_t>(eip + 2);
                stackBase[regId] = stack[constId];
                continue;
            }
            case kLoadSlot: {
                auto regId = *(eip + 1);
                auto constId = readFromMemory<uint16_t>(eip + 2);
                SlotAccessor* slot = (SlotAccessor*)stack[constId].val;
                stackBase[regId] = slot->data;
                continue;
            }
            case kMove: {
                auto dstId = *(eip + 1);
                auto srcId = *(eip + 2);
                stackBase[dstId] = stackBase[srcId];
                continue;
            }
            case kAdd: {
                auto dstReg = *(eip + 1);
                auto leftReg = *(eip + 2);
                auto rightReg = *(eip + 3);
                if (stackBase[leftReg].tag == kTagNothing ||
                    stackBase[rightReg].tag == kTagNothing) {
                    stackBase[dstReg] = makeNothing();
                } else {
                    stackBase[dstReg].val =
                        stackBase[leftReg].val + stackBase[rightReg].val;
                }
                continue;
            }
            case kFillEmpty: {
                auto dstReg = *(eip + 1);
                auto valReg = *(eip + 2);
                auto rightReg = *(eip + 3);
                if (stackBase[valReg].tag == kTagNothing) {
                    stackBase[dstReg] = stackBase[rightReg];
                } else {
                    stackBase[dstReg] = stackBase[valReg];
                }
                continue;
            }
            case kEq: {
                assert(0);
                break;
            }
            case kJmp: {
                auto offId = readFromMemory<uint16_t>(eip + 2 /* skip padding */);
                eip += offId;
                continue;
            }
            case kTestEq: {
                auto l = *(eip + 1);
                auto r = *(eip + 2);
                if (stackBase[l] == stackBase[r]) {
                    eip += kInstructionSize;
                    // Execute the following jmp instruction right here.
                    assert(*eip == kJmp);
                    auto offId = readFromMemory<uint16_t>(eip + 2);
                    eip += offId;
                } else {
                    eip += kInstructionSize;
                }
                continue;
            }
            case kTestTruthy: {
                auto v = *(eip + 1);
                // TODO: This probably has to be nicer when we do it for real.
                bool testPasses = (stackBase[v].val != 0);
                if (testPasses) {
                    eip += kInstructionSize;
                    assert(*eip == kJmp);
                    auto offId = readFromMemory<uint16_t>(eip + 2);
                    eip += offId;
                } else {
                    eip += kInstructionSize;
                }
                continue;
            }
            case kTestFalsey: {
                auto v = *(eip + 1);
                // TODO: This probably has to be nicer when we do it for real.
                bool testPasses = (stackBase[v].val == 0);
                if (testPasses) {
                    eip += kInstructionSize;
                    assert(*eip == kJmp);
                    auto offId = readFromMemory<uint16_t>(eip + 2);
                    eip += offId;
                } else {
                    eip += kInstructionSize;
                }
                continue;
            }
            }

            // Should never get here, because of uses of continue in above loop.
            assert(0);
        }
    }

    ValTagOwned result() {
        return stack[base];
    }

    Function currentFunction;
    
    std::vector<char> instructions;
    std::vector<ValTagOwned> stack;
    size_t base;
};
