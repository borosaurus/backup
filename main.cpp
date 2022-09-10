#include <iostream>
#include <variant>
#include <math.h>
#include <vector>

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

struct ValTagOwned {
    Value val;
    Tag tag;
    bool owned = false;
};
static_assert(std::is_trivially_copyable<ValTagOwned>::value);
static_assert(sizeof(ValTagOwned) == 16);

ValTagOwned makeInt(int v) {
    return ValTagOwned{Value(v), 1, false};
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
const Register kRegA = 0;
const Register kRegB = 1;
const Register kRegC = 2;
const Register kRegD = 3;


struct InstrLoadSlot {
    SlotAccessor* accessor = nullptr;
    Register reg;
};
struct InstrLoadConst {
    ValTagOwned src;
    Register reg;
};
struct InstrAdd {
    Register left;
    Register right;
};
struct InstrEq {
    Register left;
    Register right;
};

using Instr = std::variant<
    InstrLoadSlot,
    InstrLoadConst,
    InstrAdd,
    InstrEq>;

struct ByteCodeLogical {
    std::vector<Instr> instructions;
};

enum InstrCode : char {
    kLoadSlot,
    kLoadConst,
    kAdd,
    kEq
};

struct ExecInstructions {
    void append(InstrLoadSlot instr) {
        instructions.push_back(InstrCode::kLoadSlot);
        auto* addr = allocateSpace(8);
        writeToMemory<ValTagOwned*>(addr, &(instr.accessor->data));
        instructions.push_back(instr.reg);
    }

    void append(InstrLoadConst instr) {
        instructions.push_back(InstrCode::kLoadConst);
        auto* addr = allocateSpace(sizeof(ValTagOwned));
        writeToMemory<ValTagOwned>(addr, instr.src);
        instructions.push_back(instr.reg);        
    }
    void append(InstrAdd instr) {
        instructions.push_back(InstrCode::kAdd);
        instructions.push_back(instr.left);
        instructions.push_back(instr.right);
    }
    void append(InstrEq instr) {
        instructions.push_back(InstrCode::kEq);
        instructions.push_back(instr.left);
        instructions.push_back(instr.right);
    }

    char* allocateSpace(size_t size) {
        auto oldSize = instructions.size();
        instructions.resize(oldSize + size);
        return (char*)instructions.data() + oldSize;
    }
    
    std::vector<char> instructions;
};



struct Runtime {
    void run() {
        const char* eip = instructions.data();
        const char* end = instructions.data() + instructions.size();
        while (eip != end) {
            assert(eip < end);
            InstrCode instrCode = *(InstrCode*)eip;
            ++eip;
            switch(instrCode) {
            case kLoadConst: {
                auto vtOwned = readFromMemory<ValTagOwned>(eip);
                eip += sizeof(ValTagOwned);
                auto regId = *eip;
                ++eip;

                registers[regId] = vtOwned;
                break;
            }
            case kAdd: {
                std::cout << "Supposed to add\n";
                auto leftReg = *eip;
                eip++;
                auto rightReg = *eip;
                eip++;

                registers[rightReg].val += registers[leftReg].val;
                break;
            }
            case kLoadSlot: {
                std::cout << "load slot " << (void*)eip << std::endl;
                break;
            }
            }
        }
    }
    
    std::vector<char> instructions;

    std::vector<ValTagOwned> stack;
    size_t base;
    // TODO ensure this is aligned
    // TODO other stuff can go in here
    ValTagOwned registers[4];
};

int main() {
    ExecInstructions execInstructions;
    execInstructions.append(InstrLoadConst{makeInt(123), kRegA});
    execInstructions.append(InstrLoadConst{makeInt(456), kRegB});
    execInstructions.append(InstrAdd{kRegA, kRegB});

    Runtime rt{std::move(execInstructions.instructions)};

    rt.run();
    std::cout << rt.registers[1].val << " " << rt.registers[1].tag << std::endl;
    
    return 0;
}
