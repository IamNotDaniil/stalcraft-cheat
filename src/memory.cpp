#include "memory.h"
#include <iostream>
#include <fstream>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <regex>
#include <algorithm>
#include <cctype>
#include <map>
#include <iomanip>

static pid_t target_pid = 0;

namespace memory {
    bool init(const std::string& process_name) {
        return refresh_pid(process_name);
    }

    bool refresh_pid(const std::string& process_name) {
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
            
            std::transform(comm.begin(), comm.end(), comm.begin(), ::tolower);
            std::string search_name = process_name;
            std::transform(search_name.begin(), search_name.end(), search_name.begin(), ::tolower);
            
            if (comm.find(search_name) != std::string::npos) {
                target_pid = std::stoi(entry->d_name);
                closedir(dir);
                return true;
            }
        }
        closedir(dir);
        return false;
    }

    void cleanup() {
        target_pid = 0;
    }

    pid_t get_pid() { return target_pid; }

    bool read_memory(uintptr_t address, void* buffer, size_t size) {
        if (target_pid == 0) return false;
        struct iovec local_iov = { buffer, size };
        struct iovec remote_iov = { reinterpret_cast<void*>(address), size };
        ssize_t nread = process_vm_readv(target_pid, &local_iov, 1, &remote_iov, 1, 0);
        return nread == static_cast<ssize_t>(size);
    }

    bool write_memory(uintptr_t address, const void* buffer, size_t size) {
        if (target_pid == 0) return false;
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
        std::string search_name = module_name;
        std::transform(search_name.begin(), search_name.end(), search_name.begin(), ::tolower);
        
        while (std::getline(maps_file, line)) {
            std::string lower_line = line;
            std::transform(lower_line.begin(), lower_line.end(), lower_line.begin(), ::tolower);
            
            if (lower_line.find(search_name) != std::string::npos) {
                size_t pos = line.find('-');
                if (pos != std::string::npos) {
                    base = std::stoul(line.substr(0, pos), nullptr, 16);
                    break;
                }
            }
        }
        return base;
    }

    size_t get_module_size(const std::string& module_name) {
        std::string maps_path = "/proc/" + std::to_string(target_pid) + "/maps";
        std::ifstream maps_file(maps_path);
        if (!maps_file.is_open()) return 0;

        std::string line;
        uintptr_t lowest_start = UINTPTR_MAX;
        uintptr_t highest_end = 0;
        std::string search_name = module_name;
        std::transform(search_name.begin(), search_name.end(), search_name.begin(), ::tolower);
        
        while (std::getline(maps_file, line)) {
            std::string lower_line = line;
            std::transform(lower_line.begin(), lower_line.end(), lower_line.begin(), ::tolower);
            
            if (lower_line.find(search_name) != std::string::npos) {
                uintptr_t start, end;
                char perms[5], path[256];
                if (sscanf(line.c_str(), "%lx-%lx %4s %*s %*s %*s %255s", &start, &end, perms, path) >= 3) {
                    std::string path_str = path;
                    std::transform(path_str.begin(), path_str.end(), path_str.begin(), ::tolower);
                    if (path_str.find(search_name) != std::string::npos) {
                        if (start < lowest_start) lowest_start = start;
                        if (end > highest_end) highest_end = end;
                    }
                }
            }
        }
        
        if (lowest_start != UINTPTR_MAX && highest_end != 0) {
            return highest_end - lowest_start;
        }
        return 0;
    }

    std::vector<MemoryRegion> get_all_regions() {
        std::vector<MemoryRegion> regions;
        std::string maps_path = "/proc/" + std::to_string(target_pid) + "/maps";
        std::ifstream maps_file(maps_path);
        if (!maps_file.is_open()) return regions;

        std::string line;
        while (std::getline(maps_file, line)) {
            MemoryRegion region;
            uintptr_t start, end;
            char perms[5], path[512];
            
            if (sscanf(line.c_str(), "%lx-%lx %4s %*s %*s %*s %511s", &start, &end, perms, path) >= 3) {
                region.start = start;
                region.end = end;
                region.perms = perms;
                region.path = (strlen(path) > 0 && path[0] == '/') ? path : "[anon]";
                region.is_executable = (strchr(perms, 'x') != nullptr);
                regions.push_back(region);
            }
        }
        return regions;
    }
}
