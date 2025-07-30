#include "Scheduler.h"
#include <random>
#include <iostream>
#include <chrono>

Scheduler::Scheduler() = default;

Scheduler::~Scheduler() {
    shutdown();
}

void Scheduler::initialize(const Config& cfg) {
    if (is_initialized) return;

    config = cfg;
    is_initialized = true;

    memory_manager = make_unique<MemoryAllocator>(config.max_overall_mem);

    // Start main scheduler/tick thread
    scheduler_thread_handle = thread(&Scheduler::main_scheduler_loop, this);

    // Start worker threads (CPUs)
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
    if (generate_processes.exchange(true)) return; // Already running

    is_scheduler_running = true;

    if (process_generator_thread_handle.joinable()) process_generator_thread_handle.join();
    process_generator_thread_handle = thread(&Scheduler::process_generator_loop, this);
}

void Scheduler::stop_process_generation() {
    generate_processes = false;
    is_scheduler_running = false;
}

void Scheduler::add_new_process(const string& name) {
    // generating random instructions for the new process
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> instr_dist(config.min_ins, config.max_ins);
    int num_instructions = instr_dist(gen);

    

    vector<string> declared_vars;
    int potential_total = 0; // Initialize the counter for the unrolled size

    vector<Instruction> instructions = generate_instructions(num_instructions, declared_vars, 0, potential_total);

    auto now = time(nullptr);
    tm localTime;
    localtime_s(&localTime, &now);
    char buffer[100];
    strftime(buffer, sizeof(buffer), "%m/%d/%Y, %I:%M:%S %p", &localTime);
    
    auto new_proc = make_shared<Process>(next_pid++, name, move(instructions), static_cast<size_t>(potential_total), string(buffer));
    
    new_proc->memory_size = config.mem_per_proc;

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

vector<Instruction> Scheduler::generate_instructions(
    int num_instructions,
    vector<string>& declared_vars,
    int depth,
    int& potential_total_instructions // Pass the total by reference
) {
    random_device rd;
    mt19937 gen(rd());
    vector<Instruction> instructions;
    uniform_int_distribution<> type_dist(0, 9); 

    for (int i = 0; i < num_instructions; ++i) {
        if (potential_total_instructions >= config.max_ins) {
            break; 
        }

        int instruction_choice = type_dist(gen);
        bool can_generate_for = (instruction_choice == 9 && depth < 3);

        if (can_generate_for) {
            uniform_int_distribution<uint16_t> repeat_dist(2, 10);
            uint16_t repeats = repeat_dist(gen);
            
            uniform_int_distribution<> inner_instr_count_dist(2, 5); // smaller inner loops
            int inner_count = inner_instr_count_dist(gen);
            
            int inner_potential_total = 0;
            vector<Instruction> inner_instructions = generate_instructions(inner_count, declared_vars, depth + 1, inner_potential_total);

            // Check if adding this loop exceeds the max_ins from config
            if (!inner_instructions.empty() && (potential_total_instructions + (inner_potential_total * repeats) < config.max_ins)) {
                Instruction for_loop_instr;
                for_loop_instr.type = InstructionType::FOR;
                for_loop_instr.for_block = move(inner_instructions);
                for_loop_instr.for_repeats = repeats;
                instructions.push_back(move(for_loop_instr));
                
                // Increment the total by the full unrolled size of the loop
                potential_total_instructions += (inner_potential_total * repeats);
            } else {
                // It doesn't fit or inner block is empty. Generate a simple PRINT instead.
                instructions.push_back({InstructionType::PRINT, {}});
                potential_total_instructions++; // Increment by 1
            }
        }

        else { // This block handles ALL non-FOR cases
            if (instruction_choice == 5 && declared_vars.size() < 20) { // Add DECLARE, with a limit
                string new_var_name = "v" + to_string(declared_vars.size());
                declared_vars.push_back(new_var_name);
                uniform_int_distribution<uint16_t> val_dist(0, 1000);
                instructions.push_back({InstructionType::DECLARE, {new_var_name, val_dist(gen)}});
            }
            else if ((instruction_choice == 6 || instruction_choice == 7) && declared_vars.size() >= 2) { // Add ADD or SUBTRACT
                uniform_int_distribution<size_t> var_idx_dist(0, declared_vars.size() - 1);
                string src_var1 = declared_vars[var_idx_dist(gen)];
                string src_var2 = declared_vars[var_idx_dist(gen)];
                string dest_var = declared_vars[var_idx_dist(gen)];
                instructions.push_back({(instruction_choice == 6 ? InstructionType::ADD : InstructionType::SUBTRACT), {dest_var, src_var1, src_var2}});
            }
            else if (instruction_choice == 8) { // Add SLEEP
                uniform_int_distribution<uint16_t> sleep_dist(5, 20);
                instructions.push_back({InstructionType::SLEEP, {sleep_dist(gen)}});
            }
            else { // Default to PRINT for all other cases
                instructions.push_back({InstructionType::PRINT, {}});
            }
            // Increment the counter for the single instruction we just added
            potential_total_instructions++;
        }
    }
    return instructions;
}


void Scheduler::main_scheduler_loop() {
    while (!is_shutting_down) {
        if (is_scheduler_running.load()) {
            cpu_tick++;

            if (cpu_tick.load() > 0 && (cpu_tick.load() % config.quantum_cycles == 0)) {
                lock_guard<mutex> lock(process_list_mutex);
                memory_manager->generate_snapshot(cpu_tick.load(), all_processes);
            }
            
            cv.notify_all();
        }
        
        this_thread::sleep_for(chrono::milliseconds(100));
    }
}

void Scheduler::process_generator_loop() {
    while (generate_processes && !is_shutting_down) {
        int wait_ticks = config.batch_process_freq;
        for(int i = 0; i < wait_ticks && generate_processes; ++i) {
            this_thread::sleep_for(chrono::milliseconds(100)); 
        }
        if (generate_processes) {
            string proc_name = "p" + to_string(next_pid);
            add_new_process(proc_name);
            // cout << "\n[Scheduler] Generated process " << proc_name << endl;
        }
    }
}

void Scheduler::worker_thread_loop(int core_id) {
    while (!is_shutting_down) {
       shared_ptr<Process> current_process;

        {
            unique_lock<mutex> lock(queue_mutex);
            cv.wait(lock, [this] { 
                return (is_scheduler_running.load() && !ready_queue.empty()) || is_shutting_down.load(); 
            });

            if (is_shutting_down) return;

            if (!is_scheduler_running.load()) {
                continue;
            }

            current_process = ready_queue.front();
            ready_queue.pop();
        }

        // --- MEMORY ALLOCATION LOGIC ---
        // if process is not in memory, try to allocate 
        if (current_process->base_address.load() == -1) {
            int start_address = memory_manager->allocate(current_process->id, current_process->memory_size);
            if (start_address != -1) { // successful allocation
                current_process->base_address = start_address;
            } else { // allocation failed
                lock_guard<mutex> lock(queue_mutex);
                ready_queue.push(current_process);
                cv.notify_one(); 
                continue; 
            }
        }
        
        // --- PROCESS EXECUTION LOGIC ---
        active_process_count++;
        current_process->core_assigned = core_id;

        int quantum = (config.scheduler == SchedulingAlgorithm::RR) ? config.quantum_cycles : -1;
        int instructions_executed = 0;

        while (!current_process->is_finished.load() && !is_shutting_down) {
            if (current_process->is_sleeping(cpu_tick.load())) {
                this_thread::sleep_for(chrono::milliseconds(50));
                continue;
            }
            
            current_process->execute_instruction(core_id, cpu_tick.load(), config.delay_per_exec);
            instructions_executed++;

            this_thread::sleep_for(chrono::milliseconds(1));

            if (quantum != -1 && instructions_executed >= quantum) {
                break; 
            }
        }
        
        current_process->core_assigned = -1;
        active_process_count--;

        // --- POST-EXECUTION LOGIC ---
        if (current_process->is_finished.load()) { // process finished, deallocate its memory            
            memory_manager->deallocate(current_process->id);
            current_process->base_address = -1;
        } else if (!is_shutting_down) { // process not finished, put it back on the queue
            lock_guard<mutex> lock(queue_mutex);
            ready_queue.push(current_process);
        }

        cv.notify_one();
    }
}
shared_ptr<Process> Scheduler::find_process(const string& name) {
    lock_guard<mutex> lock(process_list_mutex);
    for (const auto& proc : all_processes) {
        if (proc->name == name) {
            return proc;
        }
    }
    return nullptr;
}

vector<shared_ptr<Process>> Scheduler::get_running_processes() {
    vector<shared_ptr<Process>> running;
    lock_guard<mutex> lock(process_list_mutex);
    for (const auto& proc : all_processes) {
        if (!proc->is_finished.load()) {
            running.push_back(proc);
        }
    }
    return running;
}

vector<shared_ptr<Process>> Scheduler::get_finished_processes() {
    vector<shared_ptr<Process>> finished;
    lock_guard<mutex> lock(process_list_mutex);
    for (const auto& proc : all_processes) {
        if (proc->is_finished.load()) {
            finished.push_back(proc);
        }
    }
    return finished;
}

int Scheduler::get_cores_used() {
    return active_process_count.load();
}