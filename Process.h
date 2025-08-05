#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <ctime>
#include <mutex>
#include <optional>
#include <memory>
#include "Instruction.h"

using namespace std;

class MemoryManager; 

struct MemoryViolation {
    bool occurred = false;
    int address = 0;
    time_t timestamp;
};

class Process : public std::enable_shared_from_this<Process> {
public:
    int id;
    string name;
    string creation_timestamp;
    time_t creation_time_t;

    vector<Instruction> instructions;
    
    atomic<size_t> instruction_pointer{0};
    atomic<bool> is_finished{false}; 
    atomic<bool> needs_page_fault_handling{false};
    atomic<int> faulting_address{-1}; 
    int core_assigned = -1;

    int memory_size = 0;   
    MemoryViolation mem_violation;
    static const int SYMBOL_TABLE_SIZE = 64;

    atomic<int> sleep_until_tick{0}; 

    vector<string> logs;
    mutable mutex data_mutex; 
    
    Process(int pid, const string& pname, vector<Instruction>&& inst, size_t final_total_instructions, const string& timestamp);

    void execute_instruction(MemoryManager* mem_manager, int core_id, int current_tick, int delay_per_exec);
    
    size_t get_executed_count() const;
    size_t get_total_instructions() const;
    bool is_sleeping(int current_tick) const; 

    void set_memory_violation(int address);

private:
    unordered_map<string, int> variable_offsets;
    int next_variable_offset = 0;

    size_t total_instruction_count; 

    optional<uint16_t> resolve_value(MemoryManager* mem_manager, const Value& value, int& address);
    void execute_single_instruction(const Instruction& instr, MemoryManager* mem_manager, int core_id, int current_tick);
};