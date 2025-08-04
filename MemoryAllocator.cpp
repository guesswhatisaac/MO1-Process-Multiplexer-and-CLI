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
    localtime_s(&localTime, &now); // Use localtime_s for Windows, localtime for Linux/macOS
    char buffer[100];
    strftime(buffer, sizeof(buffer), "%m/%d/%Y, %I:%M:%S %p", &localTime);

    int processes_in_memory_count = 0;
    std::map<int, std::shared_ptr<Process>> active_processes;
    for(const auto& proc : all_processes) {
        // Only consider processes that are currently in memory and not finished
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
            // End of previous block
            if (last_pid != 0 && last_pid != -1) {
                // Check if last_pid is in active_processes before accessing its name
                if (active_processes.count(last_pid)) {
                    file << "----end---- " << i - 1 << " (" << active_processes[last_pid]->name << ")\n";
                    file << "|\n";
                } else {
                    file << "----end---- " << i - 1 << " (PID " << last_pid << " - Status Unknown/Finished)\n";
                    file << "|\n";
                }
            }
            
            // Start of new block
            if (memory[i] != 0) { // If it's not a free block
                // Check if current memory[i] is in active_processes before accessing its name
                if (active_processes.count(memory[i])) {
                    file << "|\n";
                    file << "----start---- " << i << " (" << active_processes[memory[i]]->name << ")\n";
                } else {
                    // This case indicates memory[i] holds a process ID but the process is not 'active'
                    // (e.g., finished but memory not yet deallocated).
                    file << "|\n";
                    file << "----start---- " << i << " (PID " << memory[i] << " - Status Unknown/Finished)\n";
                }
            }
            last_pid = memory[i];
        }
    }
    // Handle the very last block in memory
    if (last_pid != 0) {
        if (active_processes.count(last_pid)) {
            file << "----end---- " << total_memory_size - 1 << " (" << active_processes[last_pid]->name << ")\n";
        } else {
            file << "----end---- " << total_memory_size - 1 << " (PID " << last_pid << " - Status Unknown/Finished)\n";
        }
    }

    file.close();
    
}