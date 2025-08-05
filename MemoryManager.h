#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <memory>
#include <list>
#include <optional>
#include <map>
#include <fstream>
#include <atomic>
#include <cstdint>
#include "Process.h"

struct PageTableEntry {
    bool present = false;
    bool dirty = false;
    bool accessed = false; 
    int frame_number = -1;
    long long backing_store_location = -1;
};

struct Frame {
    bool is_free = true;
    int process_id = -1;
    int page_number = -1;
};

struct PagingStats {
    std::atomic<uint64_t> page_ins{0};
    std::atomic<uint64_t> page_outs{0};
};

class MemoryManager {
public:
    MemoryManager(int total_mem_size, int frame_sz);
    ~MemoryManager();

    bool create_virtual_memory_for_process(std::shared_ptr<Process> process);
    void release_memory_for_process(std::shared_ptr<Process> process);

    std::optional<uint16_t> read_memory(std::shared_ptr<Process> process, int virtual_address);
    bool write_memory(std::shared_ptr<Process> process, int virtual_address, uint16_t value);
    
    bool handle_page_fault(std::shared_ptr<Process> process, int page_number);

    int get_total_memory() const;
    int get_used_memory() const;
    int get_free_memory() const;
    int get_active_memory() const;
    const PagingStats& get_paging_stats() const;

private:
    std::optional<int> find_free_frame();
    int evict_page_fifo();
    void load_page_into_frame(int frame_number, std::shared_ptr<Process> process, int page_number);

    int total_memory_size;
    int frame_size;
    int num_frames;

    std::vector<Frame> physical_frames;
    std::vector<uint8_t> physical_memory;
    std::map<int, std::vector<PageTableEntry>> page_tables;

    std::list<int> fifo_queue;
    
    std::string backing_store_file = "csopesy-backing-store.txt";
    std::fstream backing_store;
    long long next_backing_store_pos = 0;

    PagingStats stats;
    mutable std::mutex memory_mutex;
};