// MemoryAllocator.cpp

#include "MemoryAllocator.h"
#include "Process.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <map>
#include <filesystem>

MemoryAllocator::MemoryAllocator(int total_size)
    : total_memory_size(total_size), memory(total_size, 0) {}

int MemoryAllocator::allocate(int process_id, int size) {
    std::lock_guard<std::mutex> lock(memory_mutex);

    for (int i = 0; i <= total_memory_size - size; ++i) {
        bool block_is_free = true;
        for (int j = 0; j < size; ++j) {
            if (memory[i + j] != 0) {
                block_is_free = false;
                i += j;
                break;
            }
        }

        if (block_is_free) {
            for (int j = 0; j < size; ++j) {
                memory[i + j] = process_id;
            }
            return i;
        }
    }

    return -1;
}

void MemoryAllocator::deallocate(int process_id) {
    std::lock_guard<std::mutex> lock(memory_mutex);
    for (int i = 0; i < total_memory_size; ++i) {
        if (memory[i] == process_id) {
            memory[i] = 0;
        }
    }
}

int MemoryAllocator::calculate_external_fragmentation() {
    int free_memory_count = 0;
    for (int i = 0; i < total_memory_size; ++i) {
        if (memory[i] == 0) {
            free_memory_count++;
        }
    }
    return free_memory_count;
}

void MemoryAllocator::generate_snapshot(int quantum_cycle, const std::vector<std::shared_ptr<Process>>& all_processes) {
    std::lock_guard<std::mutex> lock(memory_mutex);

    const std::string dir_name = "memory_stamps";
    std::filesystem::create_directory(dir_name); 
    std::string filename = dir_name + "/memory_stamp_" + std::to_string(quantum_cycle) + ".txt";
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not create memory snapshot file " << filename << std::endl;
        return;
    }

    time_t now = time(nullptr);
    tm localTime;
    localtime_s(&localTime, &now);
    char buffer[100];
    strftime(buffer, sizeof(buffer), "%m/%d/%Y, %I:%M:%S %p", &localTime);

    int processes_in_memory_count = 0;
    std::map<int, std::shared_ptr<Process>> active_processes;
    for(const auto& proc : all_processes) {
        if (proc->base_address != -1 && !proc->is_finished.load()) {
            processes_in_memory_count++;
            active_processes[proc->id] = proc;
        }
    }

    file << "Timestamp: " << buffer << "\n";
    file << "Number of processes in memory: " << processes_in_memory_count << "\n";
    
    int fragmentation_bytes = calculate_external_fragmentation();
    file << "Total external fragmentation in KB: " << (fragmentation_bytes / 1024.0) << "\n\n";

    int last_pid = -1;
    for (int i = 0; i < total_memory_size; ++i) {
        if (memory[i] != last_pid) {
            if (last_pid != 0 && last_pid != -1) {
                file << "----end---- " << i - 1 << " (" << active_processes[last_pid]->name << ")\n";
                file << "|\n";
            }
            if (memory[i] != 0) {
                file << "|\n";
                file << "----start---- " << i << " (" << active_processes[memory[i]]->name << ")\n";
            }
            last_pid = memory[i];
        }
    }
    if (last_pid != 0) {
        file << "----end---- " << total_memory_size - 1 << " (" << active_processes[last_pid]->name << ")\n";
    }

    file.close();
    
}