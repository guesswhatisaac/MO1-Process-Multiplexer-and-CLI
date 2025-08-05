#include "Process.h"
#include "MemoryManager.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <memory>

Process::Process(int pid, const string& pname, vector<Instruction>&& inst, size_t final_total_instructions, const string& timestamp)
    : id(pid), name(pname), instructions(move(inst)), total_instruction_count(final_total_instructions), creation_timestamp(timestamp) {
    creation_time_t = time(nullptr);
}

void Process::set_memory_violation(int invalid_address) {
    if (!mem_violation.occurred) {
        mem_violation.occurred = true;
        mem_violation.address = invalid_address;
        mem_violation.timestamp = time(nullptr);
        is_finished = true;
        
        lock_guard<mutex> lock(data_mutex);
        logs.push_back("FATAL: Memory Access Violation. Process terminated.");
    }
}

bool Process::is_sleeping(int current_tick) const { return sleep_until_tick.load() > current_tick; }
size_t Process::get_executed_count() const { return instruction_pointer.load(); }
size_t Process::get_total_instructions() const { return total_instruction_count; }

void Process::execute_instruction(MemoryManager* mem_manager, int core_id, int current_tick, int delay_per_exec) {
    if (is_finished.load() || is_sleeping(current_tick)) {
        return;
    }
    if (instruction_pointer.load() >= instructions.size()) {
        is_finished = true;
        return;
    }

    needs_page_fault_handling = false; 

    const Instruction& instruction = instructions[instruction_pointer.load()];
    
    {
        lock_guard<mutex> lock(data_mutex);
        execute_single_instruction(instruction, mem_manager, core_id, current_tick);
    }

    if (!needs_page_fault_handling.load()) {
        instruction_pointer++;
        if (delay_per_exec > 0) {
            sleep_until_tick = current_tick + delay_per_exec;
        }
    }
    
    if (instruction_pointer.load() >= instructions.size()) {
        is_finished = true;
    }
}

optional<uint16_t> Process::resolve_value(MemoryManager* mem_manager, const Value& value, int& address_out) {
    if (holds_alternative<uint16_t>(value)) return get<uint16_t>(value);
    if (holds_alternative<int>(value)) return static_cast<uint16_t>(get<int>(value));

    const string& var_name = get<string>(value);
    if (variable_offsets.find(var_name) == variable_offsets.end()) return 0;

    address_out = variable_offsets.at(var_name);
    auto read_val = mem_manager->read_memory(shared_from_this(), address_out);
    
    if (!read_val) {
        faulting_address = address_out;
        needs_page_fault_handling = true;
        return nullopt;
    }
    return *read_val;
}

void Process::execute_single_instruction(const Instruction& instruction, MemoryManager* mem_manager, int core_id, int current_tick) {
    int dummy_address;
    
    switch (instruction.type) {
        case InstructionType::DECLARE: {
            const string& var_name = get<string>(instruction.args[0]);
            auto initial_value_opt = resolve_value(mem_manager, instruction.args[1], dummy_address);
            if (!initial_value_opt) return;
            if (variable_offsets.find(var_name) == variable_offsets.end()) {
                if (next_variable_offset + sizeof(uint16_t) > SYMBOL_TABLE_SIZE) break;
                variable_offsets[var_name] = next_variable_offset;
                next_variable_offset += sizeof(uint16_t);
            }
            int var_address = variable_offsets.at(var_name);
            if (!mem_manager->write_memory(shared_from_this(), var_address, *initial_value_opt)) {
                 faulting_address = var_address;
                 needs_page_fault_handling = true;
            }
            break;
        }
        case InstructionType::ADD:
        case InstructionType::SUBTRACT: {
            const string& dest_var = get<string>(instruction.args[0]);
            if (variable_offsets.find(dest_var) == variable_offsets.end()) break;
            auto val1_opt = resolve_value(mem_manager, instruction.args[1], dummy_address);
            if (!val1_opt) return;
            auto val2_opt = resolve_value(mem_manager, instruction.args[2], dummy_address);
            if (!val2_opt) return;
            uint16_t result = (instruction.type == InstructionType::ADD) 
                ? min((uint32_t)65535, (uint32_t)*val1_opt + *val2_opt)
                : ((*val1_opt < *val2_opt) ? 0 : *val1_opt - *val2_opt);
            int dest_address = variable_offsets.at(dest_var);
            if (!mem_manager->write_memory(shared_from_this(), dest_address, result)) {
                faulting_address = dest_address;
                needs_page_fault_handling = true;
            }
            break;
        }
        case InstructionType::READ: {
            const string& var_name = get<string>(instruction.args[0]);
            int read_address = get<int>(instruction.args[1]);
            auto value_opt = mem_manager->read_memory(shared_from_this(), read_address);
            if (!value_opt) {
                faulting_address = read_address;
                needs_page_fault_handling = true;
                return;
            }
            if (variable_offsets.find(var_name) == variable_offsets.end()) {
                if (next_variable_offset + sizeof(uint16_t) > SYMBOL_TABLE_SIZE) break;
                variable_offsets[var_name] = next_variable_offset;
                next_variable_offset += sizeof(uint16_t);
            }
            int var_address = variable_offsets.at(var_name);
            if (!mem_manager->write_memory(shared_from_this(), var_address, *value_opt)) {
                 faulting_address = var_address;
                 needs_page_fault_handling = true;
            }
            break;
        }
        case InstructionType::WRITE: {
            int write_address = get<int>(instruction.args[0]);
            auto value_opt = resolve_value(mem_manager, instruction.args[1], dummy_address);
            if (!value_opt) return;
            if (!mem_manager->write_memory(shared_from_this(), write_address, *value_opt)) {
                faulting_address = write_address;
                needs_page_fault_handling = true;
            }
            break;
        }
        case InstructionType::SLEEP: {
            if (!instruction.args.empty()) {
                auto duration_opt = resolve_value(mem_manager, instruction.args[0], dummy_address);
                if (!duration_opt) return;
                sleep_until_tick = current_tick + *duration_opt;
            }
            break;
        }
        case InstructionType::PRINT: {
             ostringstream log_stream;
             log_stream << "PRINT: ";
             if (instruction.args.empty()) {
                 log_stream << "Hello from " << name;
             } else {
                 for(const auto& arg : instruction.args) {
                    if (holds_alternative<string>(arg)) {
                        const string& str_arg = get<string>(arg);
                        if (variable_offsets.find(str_arg) == variable_offsets.end()) {
                            log_stream << str_arg;
                        } else {
                            auto val_opt = resolve_value(mem_manager, arg, dummy_address);
                            if (!val_opt) return;
                            log_stream << *val_opt;
                        }
                    } else {
                        auto val_opt = resolve_value(mem_manager, arg, dummy_address);
                        if (!val_opt) return;
                        log_stream << *val_opt;
                    }
                 }
             }
             logs.push_back(log_stream.str());
             break;
        }
        case InstructionType::FOR: {
            const vector<Instruction>& inner_block = instruction.for_block;
            uint16_t repeat_count = instruction.for_repeats;
            vector<Instruction> repeated_instructions;
            repeated_instructions.reserve(inner_block.size() * repeat_count);
            for (int i = 0; i < repeat_count; ++i) {
                repeated_instructions.insert(repeated_instructions.end(), inner_block.begin(), inner_block.end());
            }
            size_t insert_pos = instruction_pointer.load() + 1;
            instructions.insert(instructions.begin() + insert_pos, repeated_instructions.begin(), repeated_instructions.end());
            break;
        }
    }
}