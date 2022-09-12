#include <iostream>
#include <variant>
#include <math.h>
#include <vector>

#include "expression.h"

template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
Overloaded(Ts...)->Overloaded<Ts...>;

template <typename T>
T readFromMemory(const char* ptr) noexcept {
    T val;
    memcpy(&val, ptr, sizeof(T));
    return val;
}

template <typename T>
size_t writeToMemory(char* ptr, const T val) noexcept {
    memcpy(ptr, &val, sizeof(T));
    return sizeof(T);
}

using Value = uint64_t;
using Tag = uint8_t;

const Tag kTagNothing = 0;
const Tag kTagInt = 1;
const Tag kTagBool = 2;

struct ValTagOwned {
    Value val;
    Tag tag;
    bool owned = false;

    bool operator==(const ValTagOwned&) const = default;
};
static_assert(std::is_trivially_copyable<ValTagOwned>::value);
static_assert(sizeof(ValTagOwned) == 16);

ValTagOwned makeInt(int v) {
    return ValTagOwned{Value(v), kTagInt, false};
}
ValTagOwned makeNothing() {
    return ValTagOwned{0, kTagNothing, false};
}
ValTagOwned makeBool(bool b) {
    return ValTagOwned{Value(b), kTagBool, false};
}

struct ValTag {
    Value val;
    Tag tag;
};

struct SlotAccessor {
    ValTagOwned data;
};

// After register allocation
using Register = uint8_t;

struct InstrLoadSlot {
    Register reg;
    uint8_t slotNum; // TODO: Could support larger size here
};
struct InstrLoadConst {
    Register dst;
    ValTagOwned constVal;
};
struct InstrAdd {
    Register dst;
    Register left;
    Register right;
};
// Computes whether left and right are equal.
struct InstrEq {
    Register dst;
    Register left;
    Register right;
};
struct InstrFillEmpty {
    Register dst;
    Register left;
    Register right;
};

// Tests whether left and right are equal, skipping the following jump if not.
struct InstrTestEq {
    // No dest
    Register left;
    Register right;
};

struct InstrJmp {
    uint16_t off;
};

const size_t kInstructionSize = 4;

using Instr = std::variant<
    InstrLoadSlot,
    InstrLoadConst,
    InstrAdd,
    InstrEq,
    InstrFillEmpty,
    InstrTestEq,
    InstrJmp
    >;

enum InstrCode : char {
    kLoadSlot,
    kLoadConst,
    kAdd,
    kEq,
    kFillEmpty,
    kTestEq,
    kJmp,
};

struct ExecInstructions {
    void append(InstrLoadSlot instr) {
        assert(0);
    }

    void append(InstrLoadConst instr) {
        instructions.push_back(InstrCode::kLoadConst);
        instructions.push_back(instr.dst);

        // TODO: This should look up whether we already have the constant anywhere.
        constants.push_back(instr.constVal);
        
        auto* addr = allocateSpace(sizeof(uint16_t));
        const uint16_t offset16 = uint16_t(constants.size() - 1);
        writeToMemory<uint16_t>(addr, offset16);

        assert(instructions.size() % 4 == 0);
    }
    void append(InstrAdd instr) {
        instructions.push_back(InstrCode::kAdd);
        instructions.push_back(instr.dst);
        instructions.push_back(instr.left);
        instructions.push_back(instr.right);
        assert(instructions.size() % 4 == 0);
    }
    void append(InstrEq instr) {
        instructions.push_back(InstrCode::kEq);
        instructions.push_back(instr.dst);
        instructions.push_back(instr.left);
        instructions.push_back(instr.right);
    }
    void append(InstrFillEmpty instr) {
        instructions.push_back(InstrCode::kFillEmpty);
        instructions.push_back(instr.dst);
        instructions.push_back(instr.left);
        instructions.push_back(instr.right);
    }
    void append(InstrTestEq instr) {
        instructions.push_back(InstrCode::kTestEq);
        instructions.push_back(instr.left);
        instructions.push_back(instr.right);
        instructions.push_back(0); // padding
    }
    void append(InstrJmp instr) {
        instructions.push_back(InstrCode::kJmp);
        instructions.push_back(0); // padding
        auto* addr = allocateSpace(sizeof(uint16_t));
        writeToMemory<uint16_t>(addr, instr.off);

        assert(instructions.size() % 4 == 0);
    }
    
    
    char* allocateSpace(size_t size) {
        auto oldSize = instructions.size();
        instructions.resize(oldSize + size);
        return (char*)instructions.data() + oldSize;
    }
    
    std::vector<char> instructions;
    std::vector<ValTagOwned> constants;
};


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

        const char* eip = instructions.data();
        const char* end = instructions.data() + instructions.size();
        while (eip != end) {
            assert(uintptr_t(eip - instructions.data()) % 4 == 0);
            assert(eip < end);
            InstrCode instrCode = *(InstrCode*)eip;
            ++eip;

            // TODO: Increment eip by 4?
            
            switch(instrCode) {
            case kLoadConst: {
                auto regId = *eip;
                ++eip;

                auto constId = readFromMemory<uint16_t>(eip);
                eip += sizeof(uint16_t);
                
                stack[base + regId] = stack[constId];
                continue;
            }
            case kAdd: {
                auto dstReg = *eip;
                eip++;
                auto leftReg = *eip;
                eip++;
                auto rightReg = *eip;
                eip++;

                if (stack[base + leftReg].tag == kTagNothing ||
                    stack[base + rightReg].tag == kTagNothing) {
                    stack[base + dstReg] = makeNothing();
                } else {
                    stack[base + dstReg].val =
                        stack[base + leftReg].val + stack[base + rightReg].val;
                }
                continue;
            }
            case kFillEmpty: {
                auto dstReg = *eip;
                eip++;
                auto valReg = *eip;
                eip++;
                auto rightReg = *eip;
                eip++;
                if (stack[base + valReg].tag == kTagNothing) {
                    stack[base + dstReg] = stack[base + rightReg];
                } else {
                    stack[base + dstReg] = stack[base + valReg];
                }
                continue;
            }
            case kLoadSlot: {
                std::cout << "load slot " << (void*)eip << std::endl;
                break;
            }
            case kEq: {
                assert(0);
                break;
            }
            case kJmp: {
                eip++; // Skip padding
                auto offId = readFromMemory<uint16_t>(eip);
                eip += sizeof(uint16_t);
                eip += offId;
                continue;
            }
            case kTestEq: {
                auto l = *eip;
                eip++;
                auto r = *eip;
                eip++;
                // Skip padding.
                eip++;

                if (stack[base + l] == stack[base + r]) {
                    std::cout << "jumping inline\n";
                    // Execute the following jmp instruction right here.
                    assert(*eip == kJmp);
                    eip++;
                    eip++; // Skip padding

                    auto offId = readFromMemory<uint16_t>(eip);
                    eip += sizeof(uint16_t);

                    eip += offId;
                } else {
                    std::cout << "skipping jump\n";
                    eip += 4;
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



int main() {
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
