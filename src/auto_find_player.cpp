#include "memory.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <thread>
#include <chrono>
#include <cmath>

int main() {
    std::cout << "=== STALCRAFT X - Автопоиск структуры игрока ===\n";
    
    while (!memory::init("stalcraft")) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    std::cout << "Игра найдена, PID: " << memory::get_pid() << "\n";
    std::cout << "Сканируем память в поисках игроков...\n\n";
    
    auto regions = memory::get_all_regions();
    int candidates = 0;
    
    for (auto& region : regions) {
        if (region.perms.find("r") == std::string::npos) continue;
        if (region.path.find("[stack") != std::string::npos) continue;
        
        for (uintptr_t addr = region.start; addr < region.end - 128; addr += 8) {
            // Проверяем наличие здоровья (int) где-то в объекте
            int health = 0;
            int health_offset = -1;
            
            for (int off = 0x20; off < 0x50; off += 4) {
                if (memory::read_memory(addr + off, &health, sizeof(health))) {
                    if (health > 0 && health <= 100) {
                        health_offset = off;
                        break;
                    }
                }
            }
            
            if (health_offset == -1) continue;
            
            // Ищем координаты (double) где-то в объекте
            for (int coord_off = 0; coord_off < 0x60; coord_off += 8) {
                double x, y, z;
                if (!memory::read_memory(addr + coord_off, &x, sizeof(x))) continue;
                if (!memory::read_memory(addr + coord_off + 8, &y, sizeof(y))) continue;
                if (!memory::read_memory(addr + coord_off + 16, &z, sizeof(z))) continue;
                
                if (std::isnan(x) || std::isnan(y) || std::isnan(z)) continue;
                if (std::abs(x) > 10000 || std::abs(y) > 10000 || std::abs(z) > 10000) continue;
                if (std::abs(x) < 0.1 && std::abs(y) < 0.1 && std::abs(z) < 0.1) continue;
                
                // Нашли кандидата!
                candidates++;
                
                std::cout << "Кандидат #" << candidates << "\n";
                std::cout << "  Адрес: 0x" << std::hex << addr << std::dec << "\n";
                std::cout << "  Координаты: offset 0x" << std::hex << coord_off << std::dec 
                          << " = (" << x << ", " << y << ", " << z << ")\n";
                std::cout << "  Здоровье: offset 0x" << std::hex << health_offset << std::dec 
                          << " = " << health << "\n";
                
                // Проверяем команду
                int team;
                for (int team_off = 0x30; team_off < 0x50; team_off += 4) {
                    if (memory::read_memory(addr + team_off, &team, sizeof(team))) {
                        if (team == 0 || team == 1 || team == 2) {
                            std::cout << "  Команда: offset 0x" << std::hex << team_off << std::dec 
                                      << " = " << team << "\n";
                            break;
                        }
                    }
                }
                
                std::cout << "\n";
                if (candidates >= 10) break;
            }
            if (candidates >= 10) break;
        }
        if (candidates >= 10) break;
    }
    
    std::cout << "Найдено кандидатов: " << candidates << "\n";
    if (candidates > 0) {
        std::cout << "\nСкорее всего, правильная структура:\n";
        std::cout << "  - координаты: 0x20 или 0x30\n";
        std::cout << "  - здоровье: 0x2c\n";
        std::cout << "  - команда: 0x3c\n";
    }
    
    return 0;
}
