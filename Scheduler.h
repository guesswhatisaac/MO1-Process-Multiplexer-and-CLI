#pragma once
#include "Process.h"
#include "MemoryManager.h" 
#include <vector>
#include <queue>
#include <map>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <random>
#include <optional>
#include <cstdint>

using namespace std;

enum class SchedulingAlgorithm {
    FCFS,
    RR
};

struct Config {
    int num_cpu = 1;
    SchedulingAlgorithm scheduler = SchedulingAlgorithm::FCFS;
    int quantum_cycles = 10;
    int batch_process_freq = 100;
    int min_ins = 100;
    int max_ins = 500;
    int delay_per_exec = 0;

    int max_overall_mem = 16384; 
    int mem_per_frame = 256;    
    int min_mem_per_proc = 1024;
    int max_mem_per_proc = 4096;
};

class Scheduler {
public:
    Scheduler();
    ~Scheduler();

    void initialize(const Config& cfg);
    void start_process_generation();
    void stop_process_generation();
    
    void add_new_process(const string& name, int memory_size, optional<vector<Instruction>> instructions_opt);
    
    shared_ptr<Process> find_process(const string& name);
    vector<shared_ptr<Process>> get_running_processes();
    vector<shared_ptr<Process>> get_finished_processes();
    vector<shared_ptr<Process>> get_all_processes();
    int get_cores_used();
    
    MemoryManager* get_memory_manager() const;
    void shutdown();

    uint64_t get_total_ticks() const;
    uint64_t get_active_ticks() const;

private:
    void worker_thread_loop(int core_id);
    void process_generator_loop();
    void main_scheduler_loop();
    vector<Instruction> generate_instructions(int num_instructions, vector<string>& declared_vars, int depth, int& potential_total_instructions);
        
    Config config;
    atomic<bool> is_initialized{false};
    atomic<bool> is_shutting_down{false};
    atomic<bool> generate_processes{false};
    
    atomic<bool> is_scheduler_running{false};
    atomic<int> active_process_count{0};
    atomic<int> next_pid{1};

    atomic<int> cpu_tick{0};
    atomic<uint64_t> active_ticks{0};

    vector<thread> worker_threads;
    thread process_generator_thread_handle;
    thread scheduler_thread_handle;

    queue<shared_ptr<Process>> ready_queue;
    vector<shared_ptr<Process>> all_processes;

    queue<shared_ptr<Process>> page_fault_wait_queue; 
    mutex page_fault_mutex;

    unique_ptr<MemoryManager> memory_manager;

    mutex queue_mutex;
    mutex process_list_mutex;
    condition_variable cv;
};