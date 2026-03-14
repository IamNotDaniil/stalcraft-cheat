#include "pattern_scanner.h"
#include "memory.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <optional>
#include <unistd.h>
#include <sys/uio.h>
#include <map>
#include <algorithm>

namespace pattern_scanner {
    uintptr_t entity_list_addr = 0;
    uintptr_t local_player_addr = 0;
    uintptr_t health_offset = 0;
    uintptr_t team_offset = 0;
    uintptr_t visible_offset = 0;
    uintptr_t head_offset = 0;
    uintptr_t body_offset = 0;
    uintptr_t legs_offset = 0;
    uintptr_t view_angles_offset = 0;
    uintptr_t shoot_offset = 0;
    uintptr_t recoil_offset = 0;

    std::vector<ModuleInfo> g_modules;

    static bool pattern_to_bytes(const std::string& pattern, std::vector<uint8_t>& bytes, std::string& mask) {
        std::istringstream iss(pattern);
        std::string byte_str;
        while (iss >> byte_str) {
            if (byte_str == "?" || byte_str == "??") {
                bytes.push_back(0x00);
                mask += '?';
            } else {
                try {
                    uint8_t byte = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
                    bytes.push_back(byte);
                    mask += 'x';
                } catch (...) {
                    return false;
                }
            }
        }
        return !bytes.empty();
    }

    static uintptr_t find_pattern_in_region(uintptr_t start, size_t size, const std::vector<uint8_t>& pattern_bytes, const std::string& mask) {
        if (size < pattern_bytes.size()) return 0;
        
        uint8_t buffer[4096];
        
        for (size_t i = 0; i <= size - pattern_bytes.size(); i += 4096) {
            size_t chunk_size = std::min(sizeof(buffer), size - i);
            if (!memory::read_memory(start + i, buffer, chunk_size)) {
                continue;
            }
            
            for (size_t j = 0; j <= chunk_size - pattern_bytes.size(); ++j) {
                bool found = true;
                for (size_t k = 0; k < pattern_bytes.size(); ++k) {
                    if (mask[k] == 'x' && buffer[j + k] != pattern_bytes[k]) {
                        found = false;
                        break;
                    }
                }
                if (found) {
                    return start + i + j;
                }
            }
        }
        return 0;
    }

    std::optional<uintptr_t> scan_pattern(uintptr_t start, size_t size, const std::string& pattern, int32_t offset, bool relative, int deref) {
        std::vector<uint8_t> pattern_bytes;
        std::string mask;
        if (!pattern_to_bytes(pattern, pattern_bytes, mask)) {
            return std::nullopt;
        }

        uintptr_t match = find_pattern_in_region(start, size, pattern_bytes, mask);
        if (match == 0) return std::nullopt;

        uintptr_t addr = match + offset;

        if (relative) {
            int32_t rel_offset = 0;
            if (!memory::read_memory(addr, &rel_offset, sizeof(rel_offset))) {
                return std::nullopt;
            }
            addr = addr + rel_offset + sizeof(int32_t);
        }

        for (int i = 0; i < deref; ++i) {
            uintptr_t next_addr = 0;
            if (!memory::read_memory(addr, &next_addr, sizeof(next_addr))) {
                return std::nullopt;
            }
            addr = next_addr;
            if (addr == 0) return std::nullopt;
        }

        return addr;
    }

    bool get_process_modules() {
        auto regions = memory::get_all_regions();
        if (regions.empty()) return false;

        std::map<std::string, std::pair<uintptr_t, uintptr_t>> module_ranges;

        for (const auto& region : regions) {
            // Пропускаем стек и другие неинтересные области
            if (region.path.find("[stack") != std::string::npos) continue;
            if (region.path.find("[heap") != std::string::npos) continue;
            if (region.path.find("[vsyscall") != std::string::npos) continue;
            if (region.path.find("[vvar") != std::string::npos) continue;
            if (region.path.find("[vdso") != std::string::npos) continue;

            std::string module_name = region.path;
            
            // Для файлов используем имя файла
            if (region.path != "[anon]") {
                size_t last_slash = module_name.find_last_of('/');
                if (last_slash != std::string::npos) {
                    module_name = module_name.substr(last_slash + 1);
                }
            } else {
                // Для анонимной памяти группируем по правам и смежности
                module_name = "anon_" + region.perms;
            }

            // Интересуемся только исполняемыми регионами и регионами с DLL/EXE
            if (region.is_executable || 
                module_name.find(".dll") != std::string::npos || 
                module_name.find(".exe") != std::string::npos) {
                
                auto& range = module_ranges[module_name];
                if (range.first == 0 || region.start < range.first) range.first = region.start;
                if (region.end > range.second) range.second = region.end;
            }
        }

        g_modules.clear();
        for (const auto& [name, range] : module_ranges) {
            if (range.first != 0 && range.second != 0) {
                size_t size = range.second - range.first;
                if (size > 64 * 1024) { // больше 64KB
                    bool is_exec = (name.find("x") != std::string::npos || 
                                    name.find(".dll") != std::string::npos || 
                                    name.find(".exe") != std::string::npos);
                    g_modules.push_back({name, range.first, size, is_exec});
                    std::cout << "Модуль: " << name << " base=0x" << std::hex << range.first 
                              << " size=0x" << size << " (" << std::dec << size/1024/1024 
                              << " MB)" << (is_exec ? " [EXEC]" : "") << std::endl;
                }
            }
        }
        
        return !g_modules.empty();
    }

// ВРЕМЕННО замени все сигнатуры на те, что мы нашли:
    static std::vector<Signature> g_signatures = {
     { "local_player", "48 8B 05 ? ? ? ? 48 85 C0", 3, 1, true, 0 },
     { "health_offset", "8B 81 ? ? ? ? 85 C0", 2, 0, false, 0 },
     // team_offset пока не найден - пропускаем
     // visible_offset пока не найден - пропускаем
     { "head_offset", "F3 0F 10 81 ? ? ? ? F3 0F 11 44 24 20", 4, 0, false, 0 },
        { "body_offset", "F3 0F 10 81 ? ? ? ? F3 0F 11 4C 24 24", 4, 0, false, 0 },
        { "legs_offset", "F3 0F 10 81 ? ? ? ? F3 0F 11 54 24 28", 4, 0, false, 0 },
        { "view_angles_offset", "F3 0F 11 81 ? ? ? ? F3 0F 10 44 24 30", 4, 0, false, 0 },
    };

    bool find_offsets() {
        std::cout << "\n=== Поиск сигнатур ===\n";
        
        if (!get_process_modules()) {
            std::cerr << "Не удалось получить список модулей" << std::endl;
            return false;
        }

        bool all_found = true;
        for (auto& sig : g_signatures) {
            bool found = false;
            for (const auto& mod : g_modules) {
                // Пропускаем неисполняемые модули для некоторых сигнатур
                if (!mod.is_executable && sig.name.find("offset") != std::string::npos) continue;
                
                std::cout << "Ищем " << sig.name << " в " << mod.name 
                          << " (0x" << std::hex << mod.base << "-0x" << mod.base + mod.size << ")" << std::dec << std::endl;
                
                auto result = scan_pattern(mod.base, mod.size, sig.pattern, sig.offset, sig.relative, sig.deref_count);
                if (result.has_value()) {
                    sig.result = result.value();
                    std::cout << "  ✅ Найдено по адресу 0x" << std::hex << sig.result << std::dec << std::endl;
                    found = true;
                    break;
                }
            }
            if (!found) {
                std::cerr << "  ❌ Не найдено: " << sig.name << std::endl;
                all_found = false;
            }
        }

        // Заполняем глобальные переменные
        for (const auto& sig : g_signatures) {
            if (sig.name == "local_player") local_player_addr = sig.result;
            else if (sig.name == "health_offset") health_offset = sig.result;
            else if (sig.name == "team_offset") team_offset = sig.result;
        }

        return all_found;
    }
}
