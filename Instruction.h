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
    FOR
};

using Value = variant<string, uint16_t>;

struct Instruction {
    InstructionType type;
    vector<Value> args; // can be variables or literals
    
    vector<Instruction> for_block; 
    uint16_t for_repeats = 0;
};