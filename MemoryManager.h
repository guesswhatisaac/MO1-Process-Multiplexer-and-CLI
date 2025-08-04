#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <memory>
#include <atomic>    
#include <cstdint>  

class Process; 

// Tracks paging statistics for vmstat.
struct PagingStats {
    std::atomic<uint64_t> page_ins{0};
    std::atomic<uint64_t> page_outs{0};
}; 

class MemoryManager {
public:
    MemoryManager(int total_mem_size, int frame_sz);

    // Placeholder methods for compiling
    int get_total_memory() const;
    int get_used_memory() const;
    
    // Getter for the stats
    const PagingStats& get_paging_stats() const;

private:
    int total_memory_size;
    int frame_size;
    PagingStats stats;
    mutable std::mutex memory_mutex;
}; 