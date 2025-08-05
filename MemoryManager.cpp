#include "MemoryManager.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <algorithm>

MemoryManager::MemoryManager(int total_mem_size, int frame_sz)
    : total_memory_size(total_mem_size), frame_size(frame_sz) {
    if (frame_size <= 0) throw std::invalid_argument("Frame size must be positive.");
    num_frames = total_memory_size / frame_size;
    physical_frames.resize(num_frames);
    physical_memory.resize(total_memory_size, 0);

    backing_store.open(backing_store_file, std::ios::out | std::ios::trunc | std::ios::in | std::ios::binary);
    if (!backing_store.is_open()) {
        throw std::runtime_error("Could not open backing store file: " + backing_store_file);
    }
}

MemoryManager::~MemoryManager() {
    if (backing_store.is_open()) backing_store.close();
}

bool MemoryManager::create_virtual_memory_for_process(std::shared_ptr<Process> process) {
    std::lock_guard<std::mutex> lock(memory_mutex);
    if (page_tables.count(process->id)) return false;
    
    int num_pages = static_cast<int>(ceil(static_cast<double>(process->memory_size) / frame_size));
    page_tables[process->id] = std::vector<PageTableEntry>(num_pages);
    return true;
}

void MemoryManager::release_memory_for_process(std::shared_ptr<Process> process) {
    std::lock_guard<std::mutex> lock(memory_mutex);
    if (!page_tables.count(process->id)) return;

    for (const auto& pte : page_tables.at(process->id)) {
        if (pte.present) {
            physical_frames[pte.frame_number].is_free = true;
            physical_frames[pte.frame_number].process_id = -1;
            physical_frames[pte.frame_number].page_number = -1;
            fifo_queue.remove(pte.frame_number);
        }
    }
    page_tables.erase(process->id);
}

std::optional<uint16_t> MemoryManager::read_memory(std::shared_ptr<Process> process, int virtual_address) {
    if (virtual_address < 0 || virtual_address + sizeof(uint16_t) > process->memory_size) {
        process->set_memory_violation(virtual_address);
        return std::nullopt;
    }

    int page_number = virtual_address / frame_size;
    int offset = virtual_address % frame_size;
    
    std::lock_guard<std::mutex> lock(memory_mutex);
    
    if (page_tables.find(process->id) == page_tables.end() || page_number >= page_tables.at(process->id).size()) {
         process->set_memory_violation(virtual_address);
         return std::nullopt;
    }

    PageTableEntry& pte = page_tables.at(process->id).at(page_number);
    if (!pte.present) {
        return std::nullopt; 
    }
    
    int frame_address = pte.frame_number * frame_size;
    uint16_t value = *reinterpret_cast<uint16_t*>(&physical_memory[frame_address + offset]);
    return value;
}

bool MemoryManager::write_memory(std::shared_ptr<Process> process, int virtual_address, uint16_t value) {
    if (virtual_address < 0 || virtual_address + sizeof(uint16_t) > process->memory_size) {
        process->set_memory_violation(virtual_address);
        return false;
    }

    int page_number = virtual_address / frame_size;
    int offset = virtual_address % frame_size;

    std::lock_guard<std::mutex> lock(memory_mutex);

    if (page_tables.find(process->id) == page_tables.end() || page_number >= page_tables.at(process->id).size()) {
         process->set_memory_violation(virtual_address);
         return false;
    }

    PageTableEntry& pte = page_tables.at(process->id).at(page_number);
    if (!pte.present) {
        return false; 
    }

    int frame_address = pte.frame_number * frame_size;
    *reinterpret_cast<uint16_t*>(&physical_memory[frame_address + offset]) = value;
    pte.dirty = true;
    return true;
}

bool MemoryManager::handle_page_fault(std::shared_ptr<Process> process, int page_number) {
    std::lock_guard<std::mutex> lock(memory_mutex);
    
    if (page_tables.find(process->id) == page_tables.end() || page_number >= page_tables.at(process->id).size()) {
        process->set_memory_violation(page_number * frame_size); 
        return false;
    }
    
    stats.page_ins++;
    
    std::optional<int> free_frame_opt = find_free_frame();
    int frame_to_use;

    if (free_frame_opt) {
        frame_to_use = *free_frame_opt;
    } else {
        frame_to_use = evict_page_fifo();
    }
    
    load_page_into_frame(frame_to_use, process, page_number);
    return true;
}

std::optional<int> MemoryManager::find_free_frame() {
    for (int i = 0; i < num_frames; ++i) {
        if (physical_frames[i].is_free) return i;
    }
    return std::nullopt;
}

int MemoryManager::evict_page_fifo() {
    if (fifo_queue.empty()) {
        throw std::runtime_error("FIFO queue is empty, cannot evict a page.");
    }
    
    stats.page_outs++;
    int frame_to_evict = fifo_queue.front();
    fifo_queue.pop_front();

    Frame& frame = physical_frames[frame_to_evict];
    PageTableEntry& pte = page_tables.at(frame.process_id).at(frame.page_number);

    if (pte.dirty) {
        if (pte.backing_store_location == -1) {
            pte.backing_store_location = next_backing_store_pos;
            next_backing_store_pos += frame_size;
        }
        backing_store.seekp(pte.backing_store_location, std::ios::beg);
        backing_store.write(reinterpret_cast<char*>(&physical_memory[frame_to_evict * frame_size]), frame_size);
        backing_store.flush();
    }
    
    pte.present = false;
    pte.dirty = false;
    frame.is_free = true;
    
    return frame_to_evict;
}

void MemoryManager::load_page_into_frame(int frame_number, std::shared_ptr<Process> process, int page_number) {
    Frame& frame = physical_frames[frame_number];
    PageTableEntry& pte = page_tables.at(process->id).at(page_number);

    if (pte.backing_store_location != -1) {
        backing_store.seekg(pte.backing_store_location, std::ios::beg);
        backing_store.read(reinterpret_cast<char*>(&physical_memory[frame_number * frame_size]), frame_size);
    } else {
        std::fill(physical_memory.begin() + (frame_number * frame_size),
                  physical_memory.begin() + ((frame_number + 1) * frame_size), 0);
    }

    frame.is_free = false;
    frame.process_id = process->id;
    frame.page_number = page_number;
    
    pte.present = true;
    pte.frame_number = frame_number;
    
    fifo_queue.push_back(frame_number);
}

int MemoryManager::get_total_memory() const { return total_memory_size; }
int MemoryManager::get_used_memory() const {
    std::lock_guard<std::mutex> lock(memory_mutex);
    int used_frames = 0;
    for (const auto& frame : physical_frames) {
        if (!frame.is_free) used_frames++;
    }
    return used_frames * frame_size;
}
int MemoryManager::get_free_memory() const { return total_memory_size - get_used_memory(); }
const PagingStats& MemoryManager::get_paging_stats() const { return stats; }