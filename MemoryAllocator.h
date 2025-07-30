#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <memory>

class Process;

class MemoryAllocator {
public:
    MemoryAllocator(int total_size);

    int allocate(int process_id, int size);

    void deallocate(int process_id);

    void generate_snapshot(
        int quantum_cycle,
        const std::vector<std::shared_ptr<Process>>& all_processes
    );

private:
    int calculate_external_fragmentation();

    int total_memory_size;
    std::vector<int> memory;
    mutable std::mutex memory_mutex;
};