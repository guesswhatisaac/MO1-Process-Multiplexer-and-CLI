#include "MemoryManager.h"

MemoryManager::MemoryManager(int total_mem_size, int frame_sz) 
    : total_memory_size(total_mem_size), frame_size(frame_sz) {
    // Constructor logic will go here in the final step
}

// Placeholder implementation
int MemoryManager::get_total_memory() const {
    return total_memory_size;
}

// Placeholder implementation
int MemoryManager::get_used_memory() const {
    // Return 0 for now until we track used frames
    return 0;
}

// Getter for vmstat
const PagingStats& MemoryManager::get_paging_stats() const {
    return stats;
}