#pragma once
#include <string>
#include <vector>
#include <variant>
#include <unordered_map>
#include <cstdint>

using namespace std;

enum class InstructionType {
    PRINT,
    DECLARE,
    ADD,
    SUBTRACT,
    SLEEP,
    FOR,
    READ,
    WRITE
};

using Value = variant<string, uint16_t, int>;

struct Instruction {
    InstructionType type;
    vector<Value> args; 
    
    vector<Instruction> for_block; 
    uint16_t for_repeats = 0;
};