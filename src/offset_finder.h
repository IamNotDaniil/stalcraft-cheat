#pragma once
#include <cstdint>
#include <vector>
#include "memory.h"

namespace offset_finder {
    struct Offsets {
        uintptr_t local_player_addr = 0;
        uintptr_t entity_list_addr = 0;
        uintptr_t health_offset = 0;
        uintptr_t team_offset = 0;
        uintptr_t visible_offset = 0;
        uintptr_t head_offset = 0;
        uintptr_t body_offset = 0;
        uintptr_t legs_offset = 0;
        uintptr_t view_angles_offset = 0;
        uintptr_t shoot_offset = 0;
        uintptr_t recoil_offset = 0;
    };

    extern Offsets g_offsets;
    
    bool find_offsets();
    void find_local_player(const std::vector<memory::MemoryRegion>& regions);
    void find_entity_list(const std::vector<memory::MemoryRegion>& regions);  // Добавили объявление
    void find_player_offsets();
}
