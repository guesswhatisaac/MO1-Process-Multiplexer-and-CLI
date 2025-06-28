#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <ctime>
#include <mutex>
#include <stack> 
#include "Instruction.h"

using namespace std;

class Process {
public:
    int id;
    string name;
    string creation_timestamp;
    time_t creation_time_t;

    vector<Instruction> instructions;
    unordered_map<string, uint16_t> variables;
    
    // state
    atomic<size_t> instruction_pointer{0};
    atomic<bool> is_finished{false}; 
    int core_assigned = -1;

    // For sleeping
    atomic<int> sleep_until_tick{0}; 

    // for logging
    vector<string> logs;
    mutable mutex data_mutex; 

    // constructor declaration
    Process(int pid, const string& pname, vector<Instruction>&& inst, const string& timestamp);

    // cpu tick
    void execute_instruction(int core_id, int current_tick, int delay_per_exec);

    // for screen -ls output
    size_t get_executed_count() const;
    size_t get_total_instructions() const;
    bool is_sleeping(int current_tick) const; 

private:
    uint16_t resolve_value(const Value& value);
    void execute_single_instruction(const Instruction& instr, int core_id, int current_tick);
};