#include "memory.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <thread>
#include <chrono>
#include <cmath>

int main() {
    std::cout << "=== STALCRAFT X - Поиск смещения координат ===\n";
    
    while (!memory::init("stalcraft")) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    std::cout << "Игра найдена, PID: " << memory::get_pid() << "\n";
    std::cout << "Сканируем возможные смещения...\n\n";
    
    auto regions = memory::get_all_regions();
    
    // Ищем любой объект с здоровьем
    uintptr_t found_addr = 0;
    int found_health = 0;
    
    for (auto& region : regions) {
        if (region.perms.find("r") == std::string::npos) continue;
        if (region.path.find("[stack") != std::string::npos) continue;
        
        for (uintptr_t addr = region.start; addr < region.end - 64; addr += 16) {
            int health;
            if (!memory::read_memory(addr + 0x2c, &health, sizeof(health))) continue;
            if (health > 0 && health <= 100) {
                found_addr = addr;
                found_health = health;
                break;
            }
        }
        if (found_addr) break;
    }
    
    if (!found_addr) {
        std::cout << "Не удалось найти объект с здоровьем!\n";
        return 1;
    }
    
    std::cout << "Найден объект с здоровьем " << found_health << " по адресу 0x" 
              << std::hex << found_addr << std::dec << "\n\n";
    
    // Проверяем разные смещения для координат
    std::cout << "Проверка смещений (значения double):\n";
    std::cout << "Смещ |         X         |         Y         |         Z\n";
    std::cout << "-----|--------------------|--------------------|--------------------\n";
    
    for (int offset = 0; offset < 0x80; offset += 8) {
        double x, y, z;
        bool ok = true;
        
        if (!memory::read_memory(found_addr + offset, &x, sizeof(x))) ok = false;
        if (!memory::read_memory(found_addr + offset + 8, &y, sizeof(y))) ok = false;
        if (!memory::read_memory(found_addr + offset + 16, &z, sizeof(z))) ok = false;
        
        if (!ok) continue;
        
        // Фильтруем мусор
        if (std::isnan(x) || std::isnan(y) || std::isnan(z)) continue;
        if (std::abs(x) > 10000 || std::abs(y) > 10000 || std::abs(z) > 10000) continue;
        if (std::abs(x) < 0.001 && std::abs(y) < 0.001 && std::abs(z) < 0.001) continue;
        
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << offset << std::dec << " | "
                  << std::setw(18) << std::fixed << std::setprecision(3) << x << " | "
                  << std::setw(18) << y << " | "
                  << std::setw(18) << z << "\n";
    }
    
    return 0;
}
