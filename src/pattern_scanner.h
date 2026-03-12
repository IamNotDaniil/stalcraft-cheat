#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace pattern_scanner {
    struct Signature {
        std::string name;
        std::string pattern; // байты в hex, ? для wildcard, например "48 8B 0D ? ? ? ? 48 85 C9"
        int32_t offset;       // смещение от начала найденного паттерна до адреса
        int32_t deref_count;  // сколько раз разыменовывать (для получения указателя)
        bool relative;        // является ли адрес относительным (RIP-relative)
        uintptr_t result;     // найденный адрес
    };

    // Сканирует память в заданном диапазоне и возвращает адрес первого совпадения
    std::optional<uintptr_t> scan_pattern(uintptr_t start, size_t size, const std::string& pattern, int32_t offset = 0, bool relative = false, int deref = 0);

    // Загружает сигнатуры из встроенного списка и выполняет поиск
    bool find_offsets(uintptr_t module_base, size_t module_size);
    
    // Глобальные переменные с найденными адресами (будут заполнены)
    extern uintptr_t entity_list_addr;
    extern uintptr_t local_player_addr;
    extern uintptr_t health_offset;
    extern uintptr_t team_offset;
    extern uintptr_t visible_offset;
    extern uintptr_t head_offset;
    extern uintptr_t body_offset;
    extern uintptr_t legs_offset;
    extern uintptr_t view_angles_offset;
    extern uintptr_t shoot_offset;
    extern uintptr_t recoil_offset; // для no recoil
}
