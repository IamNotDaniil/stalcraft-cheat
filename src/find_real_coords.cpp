#include "memory.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <thread>
#include <chrono>
#include <cmath>

int main() {
    std::cout << "=== Поиск реальных координат ===\n";
    
    while (!memory::init("stalcraft")) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    std::cout << "PID: " << memory::get_pid() << "\n\n";
    
    const uintptr_t HEAP_START = 0x55556c818000;
    const uintptr_t HEAP_END = 0x55556ec82000;
    
    std::cout << "Сканируем heap 0x" << std::hex << HEAP_START << " - 0x" << HEAP_END << std::dec << "\n\n";
    
    int found = 0;
    
    for (uintptr_t addr = HEAP_START; addr < HEAP_END - 128; addr += 8) {
        // Ищем здоровье (0x2c от начала объекта)
        int health;
        if (!memory::read_memory(addr + 0x2c, &health, sizeof(health))) continue;
        if (health < 1 || health > 100) continue;
        
        // Теперь ищем координаты где-то рядом (в пределах 0x100 байт)
        for (int coord_off = 0x10; coord_off < 0x100; coord_off += 8) {
            double x, y, z;
            if (!memory::read_memory(addr + coord_off, &x, sizeof(x))) continue;
            if (!memory::read_memory(addr + coord_off + 8, &y, sizeof(y))) continue;
            if (!memory::read_memory(addr + coord_off + 16, &z, sizeof(z))) continue;
            
            // Фильтр: нормальные координаты
            if (std::isnan(x) || std::isnan(y) || std::isnan(z)) continue;
            if (std::abs(x) > 10000 || std::abs(y) > 10000 || std::abs(z) > 10000) continue;
            if (std::abs(x) < 0.1 && std::abs(y) < 0.1 && std::abs(z) < 0.1) continue;
            
            // Нашли!
            found++;
            std::cout << "Кандидат " << found << ":\n";
            std::cout << "  Адрес: 0x" << std::hex << addr << std::dec << "\n";
            std::cout << "  Здоровье: +0x2c = " << health << "\n";
            std::cout << "  Координаты: +0x" << std::hex << coord_off << std::dec 
                      << " = (" << x << ", " << y << ", " << z << ")\n";
            std::cout << "  Расстояние между полями: 0x" << std::hex << (coord_off - 0x2c) << std::dec << "\n\n";
            
            if (found >= 20) break;
        }
        if (found >= 20) break;
    }
    
    std::cout << "Найдено кандидатов: " << found << "\n";
    if (found > 0) {
        std::cout << "\nСреднее смещение координат от здоровья: около 0x10-0x20\n";
    }
    
    return 0;
}
