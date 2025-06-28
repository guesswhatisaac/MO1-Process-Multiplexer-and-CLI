#include "Process.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>


// implement constructor
Process::Process(int pid, const string& pname, vector<Instruction>&& inst, size_t final_total_instructions, const string& timestamp)
    : id(pid), name(pname), instructions(move(inst)), total_instruction_count(final_total_instructions), creation_timestamp(timestamp) {
    creation_time_t = time(nullptr);
}

bool Process::is_sleeping(int current_tick) const {
    return sleep_until_tick.load() > current_tick;
}

size_t Process::get_executed_count() const {
    return instruction_pointer.load();
}

size_t Process::get_total_instructions() const {
    return total_instruction_count;
}

void Process::execute_instruction(int core_id, int current_tick, int delay_per_exec) {
    if (is_finished.load() || is_sleeping(current_tick)) {
        return;
    }

    if (instruction_pointer.load() >= instructions.size()) {
        is_finished = true;
        return;
    }

    const Instruction& instruction = instructions[instruction_pointer.load()];
    
    {
        // uses data_mutex to protect shared data
        lock_guard<mutex> lock(data_mutex);
        // nathan mod: passed current_tick to execute_single_instruction
        execute_single_instruction(instruction, core_id, current_tick);
    }

    instruction_pointer++;

    if (delay_per_exec > 0) {
        sleep_until_tick = current_tick + delay_per_exec;
    }

     if (instruction_pointer.load() >= instructions.size()) {
        is_finished = true;
    }
}

// nathan mod: added add sub declare print sleep instructions (FOR: to be implemented)
void Process::execute_single_instruction(const Instruction& instruction, int core_id, int current_tick) {
    auto getCurrentTimestamp = []() {
        time_t now = time(nullptr);
        tm localTime;
        localtime_s(&localTime, &now);
        char buffer[100];
        strftime(buffer, sizeof(buffer), "%m/%d/%Y, %I:%M:%S %p", &localTime);
        return string(buffer);
    };

    switch (instruction.type) {
        case InstructionType::PRINT: {
            ostringstream log_stream;
            log_stream << "(" << getCurrentTimestamp() << ") Core " << core_id << ": ";
            
            if (instruction.args.empty()) {
                log_stream << "Hello world from " << name << "!";
            } else {
                log_stream << get<string>(instruction.args[0]);
                if (instruction.args.size() > 1) {
                    log_stream << resolve_value(instruction.args[1]);
                }
            }
            logs.push_back(log_stream.str());
            break;
        }

        case InstructionType::DECLARE: {
            if (instruction.args.size() == 2) {
                const string& varName = get<string>(instruction.args[0]);
                uint16_t value = resolve_value(instruction.args[1]);
                variables[varName] = value;
            }
            break;
        }

        case InstructionType::ADD: {
            if (instruction.args.size() == 3) {
                const string& dest_var = get<string>(instruction.args[0]);
                uint16_t val1 = resolve_value(instruction.args[1]);
                uint16_t val2 = resolve_value(instruction.args[2]);

                uint32_t result = static_cast<uint32_t>(val1) + val2; // use a larger type to check for overflow
                if (result > 65535) { // 65535 is max for uint16_t
                    variables[dest_var] = 65535; // clamp to max
                } else {
                    variables[dest_var] = static_cast<uint16_t>(result);
                }

            }

            
            break;
        }
        
        case InstructionType::SUBTRACT: {
            if (instruction.args.size() == 3) {
                const string& dest_var = get<string>(instruction.args[0]);
                uint16_t val1 = resolve_value(instruction.args[1]);
                uint16_t val2 = resolve_value(instruction.args[2]);
                if (val1 < val2) { // check underflow
                    variables[dest_var] = 0; // clamp to 0
                } else {
                    variables[dest_var] = val1 - val2;
                }
            }
            break;
        }

        case InstructionType::SLEEP: {
            if (instruction.args.size() == 1) {
                uint16_t duration = resolve_value(instruction.args[0]);
                sleep_until_tick = current_tick + duration;
            }
            break;
        }

        case InstructionType::FOR: {
            // Use for_block and for_repeats fields directly
            const vector<Instruction>& inner = instruction.for_block;
            uint16_t repeat_count = instruction.for_repeats;

            // Enforce 3-level nesting if needed (optional, since generation already limits depth)
            // We can remove this check

            vector<Instruction> repeated;
            for (int i = 0; i < repeat_count; ++i) {
                repeated.insert(repeated.end(), inner.begin(), inner.end());
            }

            // Inject repeated instructions at the current position / flatten FOR loop representation
            size_t insert_pos = instruction_pointer.load() + 1;
            instructions.insert(instructions.begin() + insert_pos, repeated.begin(), repeated.end());
            break;
        }
    }
}

uint16_t Process::resolve_value(const Value& value) {
    if (holds_alternative<string>(value)) {
        const string& varName = get<string>(value);
        // nathan mod: as per spec (declare var w/ 0 if not found)
        if (variables.find(varName) == variables.end()) {
            variables[varName] = 0;
        }
        return variables[varName];
    } else {
        return get<uint16_t>(value);
    }
}


