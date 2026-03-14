#include "offset_finder.h"
#include "memory.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <set>
#include <algorithm>

namespace offset_finder {
    Offsets g_offsets = {0};

    bool find_offsets() {
        std::cout << "\n=== Поиск оффсетов (по найденной структуре) ===\n";
        
        // Оставляем только оффсеты, адрес игрока будет найден динамически
        g_offsets.local_player_addr = 0;  // <-- ВАЖНО: обнуляем
        g_offsets.health_offset = 0x2c;
        g_offsets.team_offset = 0x3c;
        g_offsets.view_angles_offset = 0x18;
        g_offsets.body_offset = 0x0;
        g_offsets.head_offset = 0x30;
        g_offsets.legs_offset = 0x48;
        
        std::cout << "\nРезультаты:\n";
        std::cout << "local_player_addr: будет найден динамически\n";
        std::cout << "health_offset: 0x" << std::hex << g_offsets.health_offset << std::dec << "\n";
        std::cout << "team_offset: 0x" << std::hex << g_offsets.team_offset << std::dec << "\n";
        std::cout << "view_angles_offset: 0x" << std::hex << g_offsets.view_angles_offset << std::dec << "\n";
        
        return true;
    }
}
