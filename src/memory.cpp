#include "memory.h"
#include <iostream>
#include <fstream>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <regex>

static pid_t target_pid = 0;

namespace memory {
    bool init(const std::string& process_name) {
        DIR* dir = opendir("/proc");
        if (!dir) return false;

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type != DT_DIR) continue;
            if (!std::all_of(entry->d_name, entry->d_name+strlen(entry->d_name), ::isdigit)) continue;

            std::string comm_path = std::string("/proc/") + entry->d_name + "/comm";
            std::ifstream comm_file(comm_path);
            if (!comm_file.is_open()) continue;
            std::string comm;
            std::getline(comm_file, comm);
            if (comm == process_name) {
                target_pid = std::stoi(entry->d_name);
                break;
            }
        }
        closedir(dir);
        return target_pid != 0;
    }

    void cleanup() {
        target_pid = 0;
    }

    pid_t get_pid() { return target_pid; }

    bool read_memory(uintptr_t address, void* buffer, size_t size) {
        struct iovec local_iov = { buffer, size };
        struct iovec remote_iov = { reinterpret_cast<void*>(address), size };
        ssize_t nread = process_vm_readv(target_pid, &local_iov, 1, &remote_iov, 1, 0);
        return nread == static_cast<ssize_t>(size);
    }

    bool write_memory(uintptr_t address, const void* buffer, size_t size) {
        struct iovec local_iov = { const_cast<void*>(buffer), size };
        struct iovec remote_iov = { reinterpret_cast<void*>(address), size };
        ssize_t nwritten = process_vm_writev(target_pid, &local_iov, 1, &remote_iov, 1, 0);
        return nwritten == static_cast<ssize_t>(size);
    }

    uintptr_t get_module_base(const std::string& module_name) {
        std::string maps_path = "/proc/" + std::to_string(target_pid) + "/maps";
        std::ifstream maps_file(maps_path);
        if (!maps_file.is_open()) return 0;

        std::string line;
        uintptr_t base = 0;
        std::regex module_regex(R"(^([0-9a-f]+)-[0-9a-f]+\s+...\s+[0-9a-f]+\s+[0-9a-f]+:[0-9a-f]+\s+\d+\s+(.*/)?([^\n]+\.(exe|dll|so))$)");
        while (std::getline(maps_file, line)) {
            std::smatch match;
            if (std::regex_search(line, match, module_regex)) {
                if (match[3] == module_name) {
                    base = std::stoul(match[1], nullptr, 16);
                    break;
                }
            }
        }
        return base;
    }
}

size_t memory::get_module_size(const std::string& module_name) {
    std::string maps_path = "/proc/" + std::to_string(target_pid) + "/maps";
    std::ifstream maps_file(maps_path);
    if (!maps_file.is_open()) return 0;

    std::string line;
    size_t total_size = 0;
    uintptr_t last_end = 0;
    bool in_module = false;
    while (std::getline(maps_file, line)) {
        if (line.find(module_name) != std::string::npos) {
            uintptr_t start, end;
            char perms[5];
            char path[256];
            sscanf(line.c_str(), "%lx-%lx %4s %*s %*s %*s %s", &start, &end, perms, path);
            if (in_module && start != last_end) {
                // Если это новый кусок того же модуля, но с разрывом? В любом случае суммируем
                total_size += (end - start);
                last_end = end;
            } else if (!in_module) {
                in_module = true;
                total_size += (end - start);
                last_end = end;
            } else {
                // непрерывный кусок
                total_size += (end - start);
                last_end = end;
            }
        } else {
            in_module = false;
        }
    }
    return total_size;
}
