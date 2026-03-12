#include <iomanip>
#include "pattern_scanner.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <optional>
#include <unistd.h>
#include <sys/uio.h>
#include "memory.h"

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

    // Преобразование строки паттерна в байты и маску
    static bool pattern_to_bytes(const std::string& pattern, std::vector<uint8_t>& bytes, std::string& mask) {
        std::istringstream iss(pattern);
        std::string byte_str;
        while (iss >> byte_str) {
            if (byte_str == "?" || byte_str == "??") {
                bytes.push_back(0x00);
                mask += '?';
            } else {
                uint8_t byte = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
                bytes.push_back(byte);
                mask += 'x';
            }
        }
        return !bytes.empty();
    }

    // Поиск сигнатуры в регионе памяти
    static uintptr_t find_pattern_in_region(uintptr_t start, size_t size, const std::vector<uint8_t>& pattern_bytes, const std::string& mask) {
        if (size < pattern_bytes.size()) return 0;
        for (size_t i = 0; i <= size - pattern_bytes.size(); ++i) {
            bool found = true;
            for (size_t j = 0; j < pattern_bytes.size(); ++j) {
                if (mask[j] == 'x' && pattern_bytes[j] != *reinterpret_cast<uint8_t*>(start + i + j)) {
                    found = false;
                    break;
                }
            }
            if (found) {
                return start + i;
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

        // Применяем смещение
        uintptr_t addr = match + offset;

        // Если относительный адрес (RIP-relative), читаем 4-байтовое смещение и добавляем к текущему адресу
        if (relative) {
            int32_t rel_offset = 0;
            if (!memory::read_memory(addr, &rel_offset, sizeof(rel_offset))) {
                return std::nullopt;
            }
            addr = addr + rel_offset + sizeof(int32_t); // +4 потому что смещение считается от следующей инструкции
        }

        // Разыменовываем нужное количество раз
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

    // Здесь нужно определить реальные сигнатуры для Stalcraft X
    // ТЕБЕ НУЖНО НАЙТИ ИХ САМОМУ! Это примеры.
    // Используй IDA Pro или другой дизассемблер, чтобы найти уникальные последовательности байт.
    static std::vector<Signature> g_signatures = {
        // Пример: поиск указателя на список сущностей (обычно это адрес, который загружается в регистр при обращении)
        { "entity_list", "48 8B 0D ? ? ? ? 48 85 C9 74 2A", 3, 1, true, 0 }, // mov rcx, [rip+???]
        { "local_player", "48 8B 05 ? ? ? ? 48 85 C0 74 10", 3, 1, true, 0 },
        // Оффсеты для структуры игрока можно найти, анализируя функцию, которая читает здоровье
        { "health_offset", "8B 81 ? ? ? ? 85 C0 7E 1F", 2, 0, false, 0 }, // mov eax, [rcx+???] ; это смещение здоровья
        { "team_offset", "8B 81 ? ? ? ? 3B F0", 2, 0, false, 0 },
        { "visible_offset", "0F B6 81 ? ? ? ? 84 C0", 3, 0, false, 0 },
        { "head_offset", "F3 0F 10 81 ? ? ? ? F3 0F 11 44 24 20", 4, 0, false, 0 }, // movss xmm0, [rcx+???] (head pos)
        { "body_offset", "F3 0F 10 81 ? ? ? ? F3 0F 11 4C 24 24", 4, 0, false, 0 },
        { "legs_offset", "F3 0F 10 81 ? ? ? ? F3 0F 11 54 24 28", 4, 0, false, 0 },
        { "view_angles_offset", "F3 0F 11 81 ? ? ? ? F3 0F 10 44 24 30", 4, 0, false, 0 },
        { "shoot_offset", "C6 81 ? ? ? ? 01 8B 81 ? ? ? ?", 2, 0, false, 0 }, // mov byte ptr [rcx+???], 1 (shoot flag)
        { "recoil_offset", "F3 0F 11 81 ? ? ? ? 8B 81 ? ? ? ?", 4, 0, false, 0 },
    };

    bool find_offsets(uintptr_t module_base, size_t module_size) {
        bool all_found = true;
        for (auto& sig : g_signatures) {
            auto result = scan_pattern(module_base, module_size, sig.pattern, sig.offset, sig.relative, sig.deref_count);
            if (result.has_value()) {
                sig.result = result.value();
                std::cout << "Found " << sig.name << ": 0x" << std::hex << sig.result << std::dec << std::endl;
            } else {
                std::cerr << "Failed to find signature: " << sig.name << std::endl;
                all_found = false;
            }
        }

        // Заполняем глобальные переменные
        for (const auto& sig : g_signatures) {
            if (sig.name == "entity_list") entity_list_addr = sig.result;
            else if (sig.name == "local_player") local_player_addr = sig.result;
            else if (sig.name == "health_offset") health_offset = sig.result;
            else if (sig.name == "team_offset") team_offset = sig.result;
            else if (sig.name == "visible_offset") visible_offset = sig.result;
            else if (sig.name == "head_offset") head_offset = sig.result;
            else if (sig.name == "body_offset") body_offset = sig.result;
            else if (sig.name == "legs_offset") legs_offset = sig.result;
            else if (sig.name == "view_angles_offset") view_angles_offset = sig.result;
            else if (sig.name == "shoot_offset") shoot_offset = sig.result;
            else if (sig.name == "recoil_offset") recoil_offset = sig.result;
        }

        return all_found;
    }
}
