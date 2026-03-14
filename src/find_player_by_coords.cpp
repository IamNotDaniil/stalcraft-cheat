#include "memory.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <thread>
#include <chrono>
#include <cmath>

int main() {
    std::cout << "=== STALCRAFT X - Поиск игрока по координатам ===\n";
    
    while (!memory::init("stalcraft")) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    std::cout << "Игра найдена, PID: " << memory::get_pid() << "\n";
    std::cout << "Введи свои примерные координаты X Y Z (например: 100 200 50): ";
    
    double target_x, target_y, target_z;
    std::cin >> target_x >> target_y >> target_z;
    
    std::cout << "\nИщем объекты с координатами близкими к (" 
              << target_x << ", " << target_y << ", " << target_z << ")\n\n";
    
    auto regions = memory::get_all_regions();
    int found_count = 0;
    
    for (auto& region : regions) {
        if (region.perms.find("r") == std::string::npos) continue;
        if (region.path.find("[stack") != std::string::npos) continue;
        
        for (uintptr_t addr = region.start; addr < region.end - 64; addr += 16) {
            // Пробуем разные смещения
            for (int offset = 0; offset < 0x80; offset += 8) {
                double x, y, z;
                if (!memory::read_memory(addr + offset, &x, sizeof(x))) continue;
                if (!memory::read_memory(addr + offset + 8, &y, sizeof(y))) continue;
                if (!memory::read_memory(addr + offset + 16, &z, sizeof(z))) continue;
                
                if (std::isnan(x) || std::isnan(y) || std::isnan(z)) continue;
                if (std::abs(x) > 10000 || std::abs(y) > 10000 || std::abs(z) > 10000) continue;
                
                // Проверяем, похоже ли на координаты игрока
                double dx = std::abs(x - target_x);
                double dy = std::abs(y - target_y);
                double dz = std::abs(z - target_z);
                
                if (dx < 50 && dy < 50 && dz < 50) {
                    int health = 0;
                    memory::read_memory(addr + 0x2c, &health, sizeof(health));
                    
                    std::cout << "Найдено! Адрес: 0x" << std::hex << addr << std::dec << "\n";
                    std::cout << "  Смещение координат: 0x" << std::hex << offset << std::dec << "\n";
                    std::cout << "  Позиция: (" << x << ", " << y << ", " << z << ")\n";
                    std::cout << "  Здоровье: " << health << "\n";
                    std::cout << "  Отклонение: " << dx << ", " << dy << ", " << dz << "\n\n";
                    
                    found_count++;
                    if (found_count >= 5) break;
                }
            }
            if (found_count >= 5) break;
        }
        if (found_count >= 5) break;
    }
    
    if (found_count == 0) {
        std::cout << "Ничего не найдено. Попробуй другие координаты.\n";
    }
    
    return 0;
}
