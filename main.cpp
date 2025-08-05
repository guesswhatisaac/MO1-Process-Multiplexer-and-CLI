#include <iostream>
#include <list>
#include <ctime>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <vector>
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <memory>
#include <optional>
#include "Scheduler.h"
#include "MemoryManager.h"

using namespace std;

// COLOR CODES =============================================================================================================
#define CYAN "\033[36m" 
#define BLUE "\033[94m"
#define BRIGHTGREEN "\033[92m"
#define BRIGHTYELLOW "\033[93m"
#define RESET "\033[0m"

// FUNCTION DECLARATIONS ===================================================================================================
void print_header();
void initialize(Scheduler& scheduler, Config& config, bool& initialized);
void report_util(Scheduler& scheduler, const Config& config);
void clear();
void display_process_screen(shared_ptr<Process> process);
void list_screens(Scheduler& scheduler, const Config& config);
void process_smi(Scheduler& scheduler);
void vmstat(Scheduler& scheduler, const Config& config);
bool is_power_of_two(int n);
vector<Instruction> parse_instructions_from_string(const string& raw_instructions, int& error_code);
string get_timestamp_from_time_t(time_t time);

// MAIN PROGRAM ============================================================================================================
int main() {
    Scheduler scheduler;
    Config config;
    bool initialized = false;

    system("cls");
    print_header();

    string input;
    while (true) {
        cout << BRIGHTYELLOW << "[main] Enter command: " << RESET;
        getline(cin, input);

        if (cin.eof()) {
             break;
        }

        stringstream ss(input);
        string command;
        ss >> command;

        if (!initialized && command != "initialize" && command != "exit") {
            cout << "Please enter the command 'initialize' before using any other command.\n";
            continue;
        }

        if (command == "initialize") {
            string junk;
            if (ss >> junk) {
                cout << "Initialize command takes no arguments. Please try again.\n";
            } else {
                initialize(scheduler, config, initialized);
            }
        }
        else if (command == "screen") {
            string opt;
            if (!(ss >> opt)) {
                cout << "Please specify a screen option (e.g., -s, -c, -r, -ls).\n";
                continue;
            }
            if (opt == "-s" || opt == "-c") {
                string name, size_str;
                if (!(ss >> name >> size_str)) {
                    cout << "Usage: screen " << opt << " <name> <size>" << (opt == "-c" ? " \"<instructions>\"" : "") << "\n";
                    continue;
                }
                int mem_size;
                try { mem_size = stoi(size_str); } catch(...) { cout << "Invalid memory size specified.\n"; continue; }

                if (mem_size < 64 || mem_size > 65536 || !is_power_of_two(mem_size)) {
                    cout << "Invalid memory allocation. Size must be a power of 2 between 64 and 65536.\n";
                } else if (scheduler.find_process(name)) {
                    cout << "Screen '" << name << "' already exists.\n";
                } else {
                    if (opt == "-s") {
                        scheduler.add_new_process(name, mem_size, nullopt);
                        cout << "Screen '" << name << "' created with " << mem_size << " bytes of memory.\n";
                    } else { // -c
                        string instruction_str;
                        getline(ss, instruction_str);
                        size_t first = instruction_str.find_first_not_of(" \t\"");
                        size_t last = instruction_str.find_last_not_of(" \t\"");
                        if (string::npos != first && string::npos != last) {
                            instruction_str = instruction_str.substr(first, (last - first + 1));
                        } else { instruction_str = ""; }
                        if (instruction_str.empty()) { cout << "Usage: screen -c <name> <size> \"<instructions>\"\n"; continue; }
                        int error_code = 0;
                        vector<Instruction> instructions = parse_instructions_from_string(instruction_str, error_code);
                        if (error_code == 1) {
                            cout << "Invalid command: Instruction count must be between 1 and 50.\n";
                        } else {
                            scheduler.add_new_process(name, mem_size, instructions);
                            cout << "Screen '" << name << "' created with custom instructions.\n";
                        }
                    }
                }
            }
            // THIS DOESNT SHOW LOGS WHEN FINISHED (mod below)
            // else if (opt == "-r") {
            //     string name;
            //     if (!(ss >> name)) { cout << "Usage: screen -r <process_name>\n"; continue; }
            //     auto process = scheduler.find_process(name);
            //     if (process) {
            //         if (process->mem_violation.occurred) {
            //             tm localTime;
            //             localtime_s(&localTime, &process->mem_violation.timestamp);
            //             char buffer[10];
            //             strftime(buffer, sizeof(buffer), "%H:%M:%S", &localTime);
            //             cout << "Process <" << name << "> shut down due to memory access violation error at " << buffer << ". ";
            //             cout << "0x" << hex << process->mem_violation.address << dec << " invalid.\n";
            //         } else if (process->is_finished.load()) {
            //             cout << "Process <" << name << "> has finished execution.\n";
            //         } else {
            //             display_process_screen(process);
            //         }
            //     } else {
            //         cout << "Process <" << name << "> not found.\n";
            //     }
            // }
            else if (opt == "-r") {
                string name;
                if (!(ss >> name)) {
                    cout << "Usage: screen -r <process_name>\n";
                    continue;
                }
                
                auto process = scheduler.find_process(name);
                if (process) {
                    if (process->mem_violation.occurred) {
                        tm localTime;
                        localtime_s(&localTime, &process->mem_violation.timestamp);
                        char buffer[10];
                        strftime(buffer, sizeof(buffer), "%H:%M:%S", &localTime);
                        
                        cout << "Process <" << name << "> shut down due to memory access violation error at " << buffer << ". ";
                        cout << "0x" << hex << process->mem_violation.address << dec << " invalid.\n";
                    } else {
                        display_process_screen(process);
                    }
                } else {
                    cout << "Process <" << name << "> not found.\n";
                }
            }
            else if (opt == "-ls") {
                string junk;
                if (ss >> junk) { cout << "Screen -ls does not take any additional arguments.\n"; } 
                else { list_screens(scheduler, config); }
            }
            else { cout << "Unknown screen command: " << opt << ". Use -s, -c, -r, or -ls.\n"; }
        }
        else if (command == "scheduler-start") {
            scheduler.start_process_generation();
            cout << "Starting process generation...\n";
        }
        else if (command == "scheduler-stop") {
            scheduler.stop_process_generation();
            cout << "Stopping process generation...\n";
        }
        else if (command == "report-util") { report_util(scheduler, config); }
        else if (command == "process-smi") { process_smi(scheduler); }
        else if (command == "vmstat") { vmstat(scheduler, config); }
        else if (command == "clear") { clear(); }
        else if (command == "exit") { break; }
        else if (!command.empty()) { cout << "Unknown command: " << command << ". Please try again." << endl; }
    }

    cout << "Shutting down scheduler and worker threads..." << endl;
    scheduler.shutdown();
    cout << "Shutdown complete. Exiting." << endl;
    return 0;
}

// SCREEN FUNCTION DEFINITIONS ===================================================================================================
string get_timestamp_from_time_t(time_t time) {
    tm localTime;
    localtime_s(&localTime, &time);
    char buffer[100];
    strftime(buffer, sizeof(buffer), "%m/%d/%Y, %I:%M:%S %p", &localTime);
    return string(buffer);
}

void display_process_screen(shared_ptr<Process> process) {
    string sub_command;
    while(true) {
        system("cls");
        lock_guard<mutex> lock(process->data_mutex);
        cout << "Process name: " << process->name << "\n";
        cout << "ID: " << process->id << "\n";
        cout << "Logs:\n";
        for(const auto& log : process->logs) {
            cout << log << "\n";
        }
        cout << "\nCurrent instruction line: " << process->get_executed_count() << "\n";
        cout << "Lines of code: " << process->get_total_instructions() << "\n\n";
        if (process->is_finished.load()) { cout << "Finished!\n\n"; }
        cout << CYAN << "> " << RESET;
        getline(cin, sub_command);
        if (sub_command == "exit") { clear(); break; }
        else if (!sub_command.empty()){
            cout << "Unknown command inside process screen. Type 'exit' to return.\n";
            this_thread::sleep_for(chrono::seconds(2));
        }
    }
}

void list_screens(Scheduler& scheduler, const Config& config) {
    auto running = scheduler.get_running_processes();
    auto finished = scheduler.get_finished_processes();
    int cores_used = scheduler.get_cores_used();
    float utilization = (config.num_cpu > 0) ? (static_cast<float>(cores_used) / config.num_cpu) * 100 : 0;
    cout << "----------------------------------------\n";
    cout << "CPU utilization: " << fixed << setprecision(2) << utilization << "%\n";
    cout << "Cores used: " << cores_used << "\n";
    cout << "Cores available: " << config.num_cpu - cores_used << "\n\n";
    cout << BRIGHTGREEN << "Running processes:\n" << RESET;
    for (const auto& proc : running) {
        cout << left << setw(12) << proc->name 
             << " (" << get_timestamp_from_time_t(proc->creation_time_t) << ")"
             << "  Core: " << (proc->core_assigned == -1 ? "wait" : to_string(proc->core_assigned))
             << "   " << proc->get_executed_count() << " / " << proc->get_total_instructions() << "\n";
    }
    cout << "\n" BRIGHTGREEN << "Finished processes:\n" << RESET;
    for (const auto& proc : finished) {
        cout << left << setw(12) << proc->name
             << " (" << get_timestamp_from_time_t(proc->creation_time_t) << ")"
             << "  Finished   "
             << proc->get_total_instructions() << " / " << proc->get_total_instructions() << "\n";
    }
    cout << "----------------------------------------\n\n";
}

// BASIC AND NEW COMMAND FUNCTION DEFINITIONS ==============================================================================
void print_header() {
    cout << "\n\n";

    cout << CYAN "  /$$$$$$   /$$$$$$   /$$$$$$  /$$$$$$$  /$$$$$$$$  /$$$$$$  /$$     /$$\n"
        << " /$$__  $$ /$$__  $$ /$$__  $$| $$__  $$| $$_____/ /$$__  $$|  $$   /$$/\n"
        << "| $$  \\__/| $$  \\__/| $$  \\ $$| $$  \\ $$| $$      | $$  \\__/ \\  $$ /$$/ \n"
        << "| $$      |  $$$$$$ | $$  | $$| $$$$$$$/| $$$$$   |  $$$$$$   \\  $$$$/  \n"
        << "| $$       \\____  $$| $$  | $$| $$____/ | $$__/    \\____  $$   \\  $$/   \n"
        << "| $$    $$ /$$  \\ $$| $$  | $$| $$      | $$       /$$  \\ $$    | $$    \n"
        << "|  $$$$$$/|  $$$$$$/|  $$$$$$/| $$      | $$$$$$$$|  $$$$$$/    | $$    \n"
        << " \\______/  \\______/  \\______/ |__/      |________/ \\______/     |__/    \n" BLUE;

    cout << "  ________  ____  __  _____    ____  \n"
        << " / ___/ _ \\/ __ \\/ / / / _ \\  |_  /  \n"
        << "/ (_ / , _/ /_/ / /_/ / ___/ _/_ <   \n"
        << "\\___/_/|_|\\____/\\____/_/    /____/   \n" RESET;

    cout << "\n\n";
    cout << BRIGHTGREEN "Hello! Welcome to Group 3's CSOPESY command line!" BRIGHTYELLOW << endl;
    cout << BRIGHTGREEN "--------------------------------------------------" BRIGHTYELLOW << endl;
    cout << "Developers: " << endl;
    cout << "> Arcega, Alexis Bea" << endl;
    cout << "> Borlaza, Clarence Bryant" << endl;
    cout << "> Campos, Annika Dominique " << endl;
    cout << "> Roman, Isaac Nathan" << endl;

    cout << BRIGHTGREEN "--------------------------------------------------" BRIGHTYELLOW << endl;

    cout << "Type 'initialize', then 'exit' to quit, 'clear' to clear the screen" RESET << endl;
    cout << endl;
}

void initialize(Scheduler& scheduler, Config& config, bool& initialized) {
    ifstream file("config.txt");
    if (!file.is_open()) { cout << "Error: Could not open config.txt\n"; return; }
    string key, value_str;
    while (file >> key) {
        if (key == "num-cpu") file >> config.num_cpu;
        else if (key == "scheduler") { file >> value_str; config.scheduler = (value_str == "\"rr\"" || value_str == "rr") ? SchedulingAlgorithm::RR : SchedulingAlgorithm::FCFS; }
        else if (key == "quantum-cycles") file >> config.quantum_cycles;
        else if (key == "batch-process-freq") file >> config.batch_process_freq;
        else if (key == "min-ins") file >> config.min_ins;
        else if (key == "max-ins") file >> config.max_ins;
        else if (key == "delay-per-exec") file >> config.delay_per_exec;
        else if (key == "max-overall-mem") file >> config.max_overall_mem;
        else if (key == "mem-per-frame") file >> config.mem_per_frame;
        else if (key == "min-mem-per-proc") file >> config.min_mem_per_proc;
        else if (key == "max-mem-per-proc") file >> config.max_mem_per_proc;
    }
    file.close();
    scheduler.initialize(config);
    initialized = true;
    cout << "\nSystem initialized successfully with config from config.txt\n\n";
}

void report_util(Scheduler& scheduler, const Config& config) {
    ofstream report_file("csopesy-log.txt");
    if (!report_file.is_open()) {
        cout << "Error: Could not open csopesy-log.txt for writing.\n";
        return;
    }

    auto running = scheduler.get_running_processes();
    auto finished = scheduler.get_finished_processes();
    int cores_used = scheduler.get_cores_used();
    float utilization = (config.num_cpu > 0) ? (static_cast<float>(cores_used) / config.num_cpu) * 100 : 0;

    report_file << "CPU utilization: " << fixed << setprecision(2) << utilization << "%\n";
    report_file << "Cores used: " << cores_used << "\n";
    report_file << "Cores available: " << config.num_cpu - cores_used << "\n\n";

    report_file << "Running processes:\n";
    for (const auto& proc : running) {
        report_file << left << setw(12) << proc->name 
             << " (" << get_timestamp_from_time_t(proc->creation_time_t) << ")"
             << "  Core: " << (proc->core_assigned == -1 ? "wait" : to_string(proc->core_assigned))
             << "   " << proc->get_executed_count() << " / " << proc->get_total_instructions() << "\n";
    }

    report_file << "\nFinished processes:\n";
    for (const auto& proc : finished) {
        report_file << left << setw(12) << proc->name
             << " (" << get_timestamp_from_time_t(proc->creation_time_t) << ")"
             << "  Finished   "
             << proc->get_total_instructions() << " / " << proc->get_total_instructions() << "\n";
    }
    report_file.close();
    cout << "Report generated at csopesy-log.txt!\n";
}

void clear() { system("cls"); print_header(); }

bool is_power_of_two(int n) { if (n <= 0) return false; return (n & (n - 1)) == 0; }

vector<Instruction> parse_instructions_from_string(const string& raw_instructions, int& error_code) {
    vector<Instruction> parsed;
    stringstream inst_stream(raw_instructions);
    string segment;
    while(getline(inst_stream, segment, ';')) {
        segment.erase(0, segment.find_first_not_of(" \t\n\r"));
        segment.erase(segment.find_last_not_of(" \t\n\r") + 1);
        if (segment.empty()) continue;
        stringstream single_inst_ss(segment);
        string type_str;
        single_inst_ss >> type_str;
        Instruction inst;
        if (type_str == "DECLARE") inst.type = InstructionType::DECLARE;
        else if (type_str == "ADD") inst.type = InstructionType::ADD;
        else if (type_str == "SUBTRACT") inst.type = InstructionType::SUBTRACT;
        else if (type_str == "READ") inst.type = InstructionType::READ;
        else if (type_str == "WRITE") inst.type = InstructionType::WRITE;
        else if (type_str == "PRINT") inst.type = InstructionType::PRINT;
        else continue;
        string arg;
        while(single_inst_ss >> arg) {
            if (arg.rfind("0x", 0) == 0) {
                try { inst.args.push_back(stoi(arg, nullptr, 16)); } catch(...) {}
            } else if (all_of(arg.begin(), arg.end(), ::isdigit)) {
                try { inst.args.push_back(static_cast<uint16_t>(stoi(arg))); } catch(...) {}
            } else {
                inst.args.push_back(arg);
            }
        }
        parsed.push_back(inst);
    }
    if (parsed.empty() || parsed.size() > 50) { error_code = 1; }
    return parsed;
}

void process_smi(Scheduler& scheduler) {
    MemoryManager* mem_manager = scheduler.get_memory_manager();
    if (!mem_manager) { cout << "Memory Manager not initialized." << endl; return; }
    cout << "+-----------------------------------------------------------------------------+\n";
    cout << "| Process Status and Memory Information                                       |\n";
    cout << "+-----------------------------------------------------------------------------+\n";
    int total_mem = mem_manager->get_total_memory();
    int used_mem = mem_manager->get_used_memory();
    float util = (total_mem > 0) ? (static_cast<float>(used_mem) / total_mem) * 100 : 0;
    stringstream mem_ss;
    mem_ss << "| Memory Usage: " << used_mem << "B / " << total_mem << "B (" << fixed << setprecision(2) << util << "%)";
    string mem_str = mem_ss.str();
    cout << mem_str << string(78 - mem_str.length(), ' ') << "|\n";
    cout << "+-----------------------+---------+------------------+------------------------+\n";
    cout << "| Process Name          | PID     | Virt. Memory (B) | Status                 |\n";
    cout << "+-----------------------+---------+------------------+------------------------+\n";
    for (const auto& proc : scheduler.get_all_processes()) {
        string status = "Finished";
        if (proc->mem_violation.occurred) { status = "MEM_FAULT"; }
        else if (!proc->is_finished.load()) { status = (proc->core_assigned != -1) ? "Running" : "Waiting/Ready"; }
        cout << "| " << left << setw(22) << proc->name << "| " << setw(8) << proc->id
             << "| " << setw(17) << proc->memory_size << "| " << setw(23) << status << "|\n";
    }
    cout << "+-----------------------+---------+------------------+------------------------+\n";
}

void vmstat(Scheduler& scheduler, const Config& config) {
    MemoryManager* mem_manager = scheduler.get_memory_manager();
    if (!mem_manager) { cout << "Error: Memory Manager not initialized." << endl; return; }
    long long total_mem_kb = config.max_overall_mem / 1024;
    long long used_mem_kb = mem_manager->get_used_memory() / 1024;
    long long free_mem_kb = total_mem_kb - used_mem_kb;
    uint64_t total_ticks = scheduler.get_total_ticks();
    uint64_t active_ticks = scheduler.get_active_ticks();
    uint64_t idle_ticks = (total_ticks > active_ticks) ? total_ticks - active_ticks : 0;
    const PagingStats& stats = mem_manager->get_paging_stats();
    uint64_t paged_in = stats.page_ins.load();
    uint64_t paged_out = stats.page_outs.load();
    cout << "\n--- System Virtual Memory Statistics ---\n";
    cout << setw(12) << right << total_mem_kb << " K total memory\n";
    cout << setw(12) << right << used_mem_kb << " K used memory\n";
    cout << setw(12) << right << free_mem_kb << " K free memory\n";
    cout << "----------------------------------------\n";
    cout << setw(12) << right << total_ticks << " total cpu ticks\n";
    cout << setw(12) << right << active_ticks << " active cpu ticks\n";
    cout << setw(12) << right << idle_ticks << " idle cpu ticks\n";
    cout << "----------------------------------------\n";
    cout << setw(12) << right << paged_in << " pages paged in\n";
    cout << setw(12) << right << paged_out << " pages paged out\n\n";
}