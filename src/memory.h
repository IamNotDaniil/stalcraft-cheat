#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <sys/uio.h>

namespace memory {
    bool init(const std::string& process_name);
    void cleanup();
    pid_t get_pid();
    bool read_memory(uintptr_t address, void* buffer, size_t size);
    bool write_memory(uintptr_t address, const void* buffer, size_t size);
    uintptr_t get_module_base(const std::string& module_name);
    size_t get_module_size(const std::string& module_name); // добавили
}
