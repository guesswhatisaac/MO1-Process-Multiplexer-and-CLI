#include "Scheduler.h"
#include "MemoryManager.h"
#include <random>
#include <iostream>
#include <chrono>
#include <cmath>

Scheduler::Scheduler() = default;

Scheduler::~Scheduler() {
    shutdown();
}

void Scheduler::initialize(const Config& cfg) {
    if (is_initialized) return;
    config = cfg;
    is_initialized = true;
    memory_manager = make_unique<MemoryManager>(config.max_overall_mem, config.mem_per_frame);
    scheduler_thread_handle = thread(&Scheduler::main_scheduler_loop, this);
    for (int i = 0; i < config.num_cpu; ++i) {
        worker_threads.emplace_back(&Scheduler::worker_thread_loop, this, i);
    }
}

void Scheduler::shutdown() {
    if (!is_shutting_down.exchange(true)) {
        stop_process_generation();
        cv.notify_all();
        if (scheduler_thread_handle.joinable()) scheduler_thread_handle.join();
        for (auto& t : worker_threads) {
            if (t.joinable()) t.join();
        }
        if (process_generator_thread_handle.joinable()) process_generator_thread_handle.join();
    }
}

void Scheduler::start_process_generation() {
    if (generate_processes.exchange(true)) return;
    is_scheduler_running = true;
    cv.notify_all();
    if (process_generator_thread_handle.joinable()) process_generator_thread_handle.join();
    process_generator_thread_handle = thread(&Scheduler::process_generator_loop, this);
}

void Scheduler::stop_process_generation() {
    generate_processes = false;
}

void Scheduler::add_new_process(const string& name, int memory_size, optional<vector<Instruction>> instructions_opt) {
    vector<Instruction> final_instructions;
    size_t total_instruction_count = 0;

    if (instructions_opt) {
        final_instructions = *instructions_opt;
        total_instruction_count = final_instructions.size();
    } else {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> instr_dist(config.min_ins, config.max_ins);
        
        int instruction_target = instr_dist(gen);
        int potential_total = 0;
        vector<string> declared_vars;
        final_instructions = generate_instructions(instruction_target, declared_vars, 0, potential_total);
        total_instruction_count = potential_total;
    }

    auto now = time(nullptr);
    tm localTime;
    localtime_s(&localTime, &now);
    char buffer[100];
    strftime(buffer, sizeof(buffer), "%m/%d/%Y, %I:%M:%S %p", &localTime);
    
    auto new_proc = make_shared<Process>(next_pid++, name, move(final_instructions), total_instruction_count, string(buffer));
    
    new_proc->memory_size = memory_size;
    memory_manager->create_virtual_memory_for_process(new_proc);

    {
        lock_guard<mutex> lock(process_list_mutex);
        all_processes.push_back(new_proc);
    }
    {
        lock_guard<mutex> lock(queue_mutex);
        ready_queue.push(new_proc);
    }
    cv.notify_one();
}

void Scheduler::main_scheduler_loop() {
    while (!is_shutting_down) {
        if (is_scheduler_running.load()) {
            cpu_tick++;
            {
                lock_guard<mutex> lock(page_fault_mutex);
                while (!page_fault_wait_queue.empty()) {
                    auto proc = page_fault_wait_queue.front();
                    page_fault_wait_queue.pop();
                    lock_guard<mutex> ready_lock(queue_mutex);
                    ready_queue.push(proc);
                }
            }
            cv.notify_all();
        }
        this_thread::sleep_for(chrono::milliseconds(100));
    }
}

void Scheduler::process_generator_loop() {
    if (config.batch_process_freq <= 0) return;
    random_device rd;
    mt19937 gen(rd());
    while (generate_processes && !is_shutting_down) {
        int wait_ticks = config.batch_process_freq;
        for(int i = 0; i < wait_ticks && generate_processes; ++i) {
            this_thread::sleep_for(chrono::milliseconds(100)); 
        }
        if (generate_processes) {
            string proc_name = "p" + to_string(next_pid);
            uniform_int_distribution<> mem_dist(config.min_mem_per_proc, config.max_mem_per_proc);
            int random_mem = mem_dist(gen);
            int mem_size = pow(2, floor(log2(random_mem)));
            add_new_process(proc_name, mem_size, nullopt);
        }
    }
}

void Scheduler::worker_thread_loop(int core_id) {
    while (!is_shutting_down) {
       shared_ptr<Process> current_process;
        {
            unique_lock<mutex> lock(queue_mutex);
            cv.wait(lock, [this] { return (is_scheduler_running.load() && !ready_queue.empty()) || is_shutting_down.load(); });
            if (is_shutting_down || !is_scheduler_running.load() || ready_queue.empty()) continue;
            current_process = ready_queue.front();
            ready_queue.pop();
        }
        active_process_count++;
        current_process->core_assigned = core_id;
        int quantum = (config.scheduler == SchedulingAlgorithm::RR) ? config.quantum_cycles : -1;
        int instructions_executed = 0;
        while (!current_process->is_finished.load() && !is_shutting_down) {
            if (current_process->is_sleeping(cpu_tick.load())) break;
            active_ticks++;
            current_process->execute_instruction(memory_manager.get(), core_id, cpu_tick.load(), config.delay_per_exec);
            if (current_process->needs_page_fault_handling.load()) {
                int page_number = current_process->faulting_address.load() / config.mem_per_frame;
                memory_manager->handle_page_fault(current_process, page_number);
                lock_guard<mutex> lock(page_fault_mutex);
                page_fault_wait_queue.push(current_process);
                break;
            }
            instructions_executed++;
            if (quantum != -1 && instructions_executed >= quantum) break;
        }
        current_process->core_assigned = -1;
        active_process_count--;
        if (current_process->is_finished.load()) {
            memory_manager->release_memory_for_process(current_process);
        } else if (!current_process->needs_page_fault_handling.load() && !is_shutting_down) {
            lock_guard<mutex> lock(queue_mutex);
            ready_queue.push(current_process);
        }
        cv.notify_one();
    }
}

vector<Instruction> Scheduler::generate_instructions(int num_instructions, vector<string>& declared_vars, int depth, int& potential_total_instructions) {
    random_device rd;
    mt19937 gen(rd());
    vector<Instruction> instructions;
    uniform_int_distribution<> type_dist(0, 9);
    for (int i = 0; i < num_instructions; ++i) {
        if (potential_total_instructions >= config.max_ins) break;
        int instruction_choice = type_dist(gen);
        bool can_generate_for = (instruction_choice == 9 && depth < 3);
        if (can_generate_for) {
            uniform_int_distribution<uint16_t> repeat_dist(2, 10);
            uint16_t repeats = repeat_dist(gen);
            uniform_int_distribution<> inner_instr_count_dist(2, 5);
            int inner_count = inner_instr_count_dist(gen);
            int inner_potential_total = 0;
            vector<Instruction> inner_instructions = generate_instructions(inner_count, declared_vars, depth + 1, inner_potential_total);
            if (!inner_instructions.empty() && (potential_total_instructions + (inner_potential_total * repeats) < config.max_ins)) {
                Instruction for_loop_instr;
                for_loop_instr.type = InstructionType::FOR;
                for_loop_instr.for_block = move(inner_instructions);
                for_loop_instr.for_repeats = repeats;
                instructions.push_back(move(for_loop_instr));
                potential_total_instructions += (inner_potential_total * repeats);
            } else {
                instructions.push_back({InstructionType::PRINT, {}});
                potential_total_instructions++;
            }
        } else {
            if (instruction_choice == 5 && declared_vars.size() < 20) {
                string new_var_name = "v" + to_string(declared_vars.size());
                declared_vars.push_back(new_var_name);
                uniform_int_distribution<uint16_t> val_dist(0, 1000);
                instructions.push_back({InstructionType::DECLARE, {new_var_name, val_dist(gen)}});
            } else if ((instruction_choice == 6 || instruction_choice == 7) && declared_vars.size() >= 2) {
                uniform_int_distribution<size_t> var_idx_dist(0, declared_vars.size() - 1);
                instructions.push_back({(instruction_choice == 6 ? InstructionType::ADD : InstructionType::SUBTRACT), {declared_vars[var_idx_dist(gen)], declared_vars[var_idx_dist(gen)], declared_vars[var_idx_dist(gen)]}});
            } else if (instruction_choice == 8) {
                uniform_int_distribution<uint16_t> sleep_dist(5, 20);
                instructions.push_back({InstructionType::SLEEP, {sleep_dist(gen)}});
            } else {
                instructions.push_back({InstructionType::PRINT, {}});
            }
            potential_total_instructions++;
        }
    }
    return instructions;
}

shared_ptr<Process> Scheduler::find_process(const string& name) {
    lock_guard<mutex> lock(process_list_mutex);
    for (const auto& proc : all_processes) {
        if (proc->name == name) return proc;
    }
    return nullptr;
}

vector<shared_ptr<Process>> Scheduler::get_running_processes() {
    vector<shared_ptr<Process>> running;
    lock_guard<mutex> lock(process_list_mutex);
    for (const auto& proc : all_processes) {
        if (!proc->is_finished.load()) running.push_back(proc);
    }
    return running;
}

vector<shared_ptr<Process>> Scheduler::get_finished_processes() {
    vector<shared_ptr<Process>> finished;
    lock_guard<mutex> lock(process_list_mutex);
    for (const auto& proc : all_processes) {
        if (proc->is_finished.load()) finished.push_back(proc);
    }
    return finished;
}

vector<shared_ptr<Process>> Scheduler::get_all_processes() {
    lock_guard<mutex> lock(process_list_mutex);
    return all_processes;
}

int Scheduler::get_cores_used() {
    return active_process_count.load();
}

MemoryManager* Scheduler::get_memory_manager() const {
    return memory_manager.get();
}

uint64_t Scheduler::get_total_ticks() const {
    return cpu_tick.load() * config.num_cpu;
}

uint64_t Scheduler::get_active_ticks() const {
    return active_ticks.load();
}