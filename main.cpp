
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
#include "Scheduler.h"

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
        string command, opt, name;
        ss >> command >> opt >> name;

        if (!initialized && command != "initialize" && command != "exit") {
            cout << "Please enter the command 'initialize' before using any other command.\n";
            continue;
        }

        if (command == "initialize") {
             if (!opt.empty() || !name.empty()) {
                 cout << "Initialize command is invalid. Please try again.\n";
             }
             else {
                 initialize(scheduler, config, initialized);
             }
        }
        else if (command == "screen" && (opt == "-s" || opt == "-r")) {
             if (name.empty()) {
                 cout << "Please provide a screen name.\n";
             }
             else {
                auto process = scheduler.find_process(name);
                if (opt == "-s") {
                    if (process) {
                        cout << "Screen '" << name << "' already exists. Use 'screen -r " << name << "' to attach.\n";
                    } else {
                        scheduler.add_new_process(name);
                        cout << "Screen '" << name << "' created.\n";
                        auto new_proc = scheduler.find_process(name);
                        if(new_proc) display_process_screen(new_proc);
                    }
                } else { // -r
                    if(process && !process->is_finished.load()) {
                        display_process_screen(process);
                    } else {
                        // If the process is null (not found) OR it is finished, print error
                        cout << "Process <" << name << "> not found.\n";
                    }
                }
             }
        }
        else if (command == "screen" && opt == "-ls") {
            if (!name.empty()) {
                cout << "Screen -ls should not include a screen name.\n";
            } else {
                list_screens(scheduler, config);
            }
        }
        else if (command == "scheduler-start") {
            cout << "Starting process generation...\n";
            scheduler.start_process_generation();
        }
        else if (command == "scheduler-stop") {
            cout << "Stopping process generation...\n";
            scheduler.stop_process_generation();
        }
        else if (command == "report-util") {
            report_util(scheduler, config);
        }
        else if (command == "clear") {
            clear();
        }
        else if (command == "exit") {
            break;
        }
        else if (!command.empty()) {
            cout << "Unknown command: " << command << ". Please try again." << endl;
        }
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

        if (process->is_finished.load()) {
            cout << "Finished!\n\n";
        }

        cout << CYAN << "> " << RESET;
        getline(cin, sub_command);

        if (sub_command == "exit") {
            clear();
            break;
        } else if (sub_command == "process-smi") {
            continue;
        } else if (!sub_command.empty()){
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


// BASIC FUNCTION DEFINITIONS ===================================================================================================
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
    if (!file.is_open()) {
        cout << "Error: Could not open config.txt\n";
        return;
    }

    string key, value_str;
    while (file >> key) {
        if (key == "num-cpu") file >> config.num_cpu;
        else if (key == "scheduler") {
            file >> value_str;
            value_str.erase(remove(value_str.begin(), value_str.end(), '\"'), value_str.end());
            config.scheduler = (value_str == "rr") ? SchedulingAlgorithm::RR : SchedulingAlgorithm::FCFS;
        }
        else if (key == "quantum-cycles") file >> config.quantum_cycles;
        else if (key == "batch-process-freq") file >> config.batch_process_freq;
        else if (key == "min-ins") file >> config.min_ins;
        else if (key == "max-ins") file >> config.max_ins;
        else if (key == "delay-per-exec") file >> config.delay_per_exec;
    }
    file.close();

    scheduler.initialize(config);
    initialized = true;

    cout << "\nSystem initialized successfully with config:\n";
    cout << "------------------------------------------\n";
    cout << "CPU cores: " << config.num_cpu << "\n";
    cout << "Scheduler: " << (config.scheduler == SchedulingAlgorithm::RR ? "rr" : "fcfs") << "\n";
    cout << "Quantum Cycles: " << config.quantum_cycles << "\n";
    cout << "Batch Process Frequency: " << config.batch_process_freq << "\n";
    cout << "Min Instructions: " << config.min_ins << "\n";
    cout << "Max Instructions: " << config.max_ins << "\n";
    cout << "Delay per Execution: " << config.delay_per_exec << "\n";
    cout << "------------------------------------------\n\n";
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

void clear() {
    system("cls");
    print_header();
}