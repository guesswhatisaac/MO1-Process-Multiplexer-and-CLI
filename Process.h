#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <ctime>
#include <mutex>
#include <optional>
#include "Instruction.h"

using namespace std;

class MemoryManager; 

struct MemoryViolation {
    bool occurred = false;
    int address = 0;
    time_t timestamp;
};


class Process {
public:
    int id;
    string name;
    string creation_timestamp;
    time_t creation_time_t;

    vector<Instruction> instructions;
    unordered_map<string, uint16_t> variables;
    
private: 
    size_t total_instruction_count; 

public:
    atomic<size_t> instruction_pointer{0};
    atomic<bool> is_finished{false}; 
    int core_assigned = -1;
    atomic<bool> needs_page_fault_handling{false}; // For memory management

    int memory_size = 0;     
    atomic<int> base_address{-1}; 
    MemoryViolation mem_violation; 

    atomic<int> sleep_until_tick{0}; 

    vector<string> logs;
    mutable mutex data_mutex; 

    Process(int pid, const string& pname, vector<Instruction>&& inst, size_t final_total_instructions, const string& timestamp);

    // mod cpu tick function declaration
    void execute_instruction(MemoryManager* mem_manager, int core_id, int current_tick, int delay_per_exec);

    size_t get_executed_count() const;
    size_t get_total_instructions() const;
    bool is_sleeping(int current_tick) const; 

    // New method to set violation status
    void set_memory_violation(int invalid_address);

private:
    uint16_t resolve_value(const Value& value);
    // mod
    void execute_single_instruction(const Instruction& instr, MemoryManager* mem_manager, int core_id, int current_tick);
};