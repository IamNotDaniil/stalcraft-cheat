#include "memory.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <cmath>
#include <thread>
#include <chrono>

struct Candidate {
    uintptr_t addr;
    int health;
    double x, y, z;
    int health_off;
    int coord_off;
};

int main() {
    std::cout << "=== ПОИСК РЕАЛЬНЫХ ОФФСЕТОВ ===\n";
    std::cout << "Встань в толпу игроков и запусти это.\n\n";
    
    while (!memory::init("stalcraft")) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    std::cout << "PID: " << memory::get_pid() << "\n";
    
    const uintptr_t HEAP_START = 0x55556c818000;
    const uintptr_t HEAP_END = 0x55556ec82000;
    
    std::vector<Candidate> candidates;
    std::map<int, int> health_offset_stats;
    std::map<int, int> coord_offset_stats;
    
    std::cout << "Сканируем память в поисках игроков...\n";
    
    for (uintptr_t addr = HEAP_START; addr < HEAP_END - 128; addr += 8) {
        
        // Пробуем разные смещения для здоровья (0x00 до 0x80)
        for (int health_off = 0; health_off < 0x80; health_off += 4) {
            int health;
            if (!memory::read_memory(addr + health_off, &health, sizeof(health))) continue;
            if (health < 1 || health > 100) continue;
            
            // Пробуем разные смещения для координат (0x00 до 0x80)
            for (int coord_off = 0; coord_off < 0x80; coord_off += 8) {
                double x, y, z;
                if (!memory::read_memory(addr + coord_off, &x, sizeof(x))) continue;
                if (!memory::read_memory(addr + coord_off + 8, &y, sizeof(y))) continue;
                if (!memory::read_memory(addr + coord_off + 16, &z, sizeof(z))) continue;
                
                if (std::isnan(x) || std::isnan(y) || std::isnan(z)) continue;
                if (std::abs(x) > 10000 || std::abs(y) > 10000 || std::abs(z) > 10000) continue;
                if (std::abs(x) < 0.1 && std::abs(y) < 0.1 && std::abs(z) < 0.1) continue;
                
                // Нашли кандидата
                Candidate c;
                c.addr = addr;
                c.health = health;
                c.x = x; c.y = y; c.z = z;
                c.health_off = health_off;
                c.coord_off = coord_off;
                
                candidates.push_back(c);
                
                health_offset_stats[health_off]++;
                coord_offset_stats[coord_off]++;
                
                if (candidates.size() % 100 == 0) {
                    std::cout << "Найдено кандидатов: " << candidates.size() << "\r" << std::flush;
                }
                
                break; // нашли координаты для этого health_off
            }
        }
    }
    
    std::cout << "\n\n=== СТАТИСТИКА ===\n";
    std::cout << "Всего кандидатов: " << candidates.size() << "\n\n";
    
    std::cout << "Топ смещений здоровья:\n";
    std::vector<std::pair<int, int>> health_stats(health_offset_stats.begin(), health_offset_stats.end());
    std::sort(health_stats.begin(), health_stats.end(), 
        [](auto& a, auto& b) { return a.second > b.second; });
    
    for (int i = 0; i < std::min(5, (int)health_stats.size()); i++) {
        std::cout << "  offset 0x" << std::hex << health_stats[i].first << std::dec 
                  << ": " << health_stats[i].second << " объектов\n";
    }
    
    std::cout << "\nТоп смещений координат:\n";
    std::vector<std::pair<int, int>> coord_stats(coord_offset_stats.begin(), coord_offset_stats.end());
    std::sort(coord_stats.begin(), coord_stats.end(), 
        [](auto& a, auto& b) { return a.second > b.second; });
    
    for (int i = 0; i < std::min(5, (int)coord_stats.size()); i++) {
        std::cout << "  offset 0x" << std::hex << coord_stats[i].first << std::dec 
                  << ": " << coord_stats[i].second << " объектов\n";
    }
    
    std::cout << "\n=== ПРИМЕРЫ ОБЪЕКТОВ ===\n";
    for (int i = 0; i < std::min(10, (int)candidates.size()); i++) {
        std::cout << "Объект " << i+1 << ":\n";
        std::cout << "  Адрес: 0x" << std::hex << candidates[i].addr << std::dec << "\n";
        std::cout << "  Здоровье: +0x" << std::hex << candidates[i].health_off 
                  << " = " << std::dec << candidates[i].health << "\n";
        std::cout << "  Координаты: +0x" << std::hex << candidates[i].coord_off 
                  << " = (" << std::fixed << std::setprecision(2) 
                  << candidates[i].x << ", " << candidates[i].y << ", " << candidates[i].z << ")\n\n";
    }
    
    return 0;
}
