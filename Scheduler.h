#pragma once
#include "Process.h"
#include <vector>
#include <queue>
#include <map>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <memory> // For shared_ptr
#include <random>

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
};

class Scheduler {
public:
    Scheduler();
    ~Scheduler();

    void initialize(const Config& cfg);
    void start_process_generation();
    void stop_process_generation();
    
    void add_new_process(const string& name);
    shared_ptr<Process> find_process(const string& name);

    vector<shared_ptr<Process>> get_running_processes();
    vector<shared_ptr<Process>> get_finished_processes();
    int get_cores_used();
    
    void shutdown();

private:
    void worker_thread_loop(int core_id);
    void process_generator_loop();
    void main_scheduler_loop();
    vector<Instruction> generate_instructions(int num_instructions, vector<string>& declared_vars, int depth, int& potential_total_instructions);
        
    Config config;
    atomic<bool> is_initialized{false};
    atomic<bool> is_shutting_down{false};
    atomic<bool> generate_processes{false};
    
    atomic<int> active_process_count{0};

    atomic<int> cpu_tick{0};
    atomic<int> next_pid{1};

    vector<thread> worker_threads;
    thread process_generator_thread_handle;
    thread scheduler_thread_handle;

    queue<shared_ptr<Process>> ready_queue;
    vector<shared_ptr<Process>> all_processes;

    mutex queue_mutex;
    mutex process_list_mutex;
    condition_variable cv;
};