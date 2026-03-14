#include "memory.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <thread>
#include <chrono>
#include <cmath>

int main() {
    std::cout << "=== Тест координат ===\n";
    
    while (!memory::init("stalcraft")) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    std::cout << "PID: " << memory::get_pid() << "\n\n";
    
    const uintptr_t HEAP_START = 0x55556c818000;
    const uintptr_t HEAP_END = 0x55556ec82000;
    
    std::vector<uintptr_t> candidates;
    
    std::cout << "Сканируем heap 0x" << std::hex << HEAP_START << " - 0x" << HEAP_END << std::dec << "\n";
    
    // Собираем кандидатов
    for (uintptr_t addr = HEAP_START; addr < HEAP_END - 64; addr += 32) {
        for (int coord_off = 0; coord_off < 0x80; coord_off += 8) {
            double x, y, z;
            if (!memory::read_memory(addr + coord_off, &x, sizeof(x))) continue;
            if (!memory::read_memory(addr + coord_off + 8, &y, sizeof(y))) continue;
            if (!memory::read_memory(addr + coord_off + 16, &z, sizeof(z))) continue;
            
            if (std::isnan(x) || std::isnan(y) || std::isnan(z)) continue;
            if (std::abs(x) > 10000 || std::abs(y) > 10000 || std::abs(z) > 10000) continue;
            if (std::abs(x) < 0.001 && std::abs(y) < 0.001 && std::abs(z) < 0.001) continue;
            
            int health;
            for (int health_off = 0x20; health_off < 0x80; health_off += 4) {
                if (!memory::read_memory(addr + health_off, &health, sizeof(health))) continue;
                if (health > 0 && health <= 100) {
                    candidates.push_back(addr);
                    std::cout << "  Найден кандидат: 0x" << std::hex << addr << std::dec 
                              << " HP=" << health << " pos=(" << x << ", " << y << ", " << z << ")\n";
                    break;
                }
            }
            break;
        }
        if (candidates.size() >= 10) break;
    }
    
    std::cout << "\nНайдено кандидатов: " << candidates.size() << "\n\n";
    
    if (candidates.empty()) {
        std::cout << "Нет кандидатов для мониторинга.\n";
        return 0;
    }
    
    // Мониторим их координаты
    for (int iter = 0; iter < 5; iter++) {
        std::cout << "=== Скан " << iter+1 << " ===\n";
        
        for (size_t i = 0; i < candidates.size(); i++) {
            uintptr_t addr = candidates[i];
            
            for (int coord_off = 0; coord_off < 0x80; coord_off += 8) {
                double x, y, z;
                if (!memory::read_memory(addr + coord_off, &x, sizeof(x))) break;
                if (!memory::read_memory(addr + coord_off + 8, &y, sizeof(y))) break;
                if (!memory::read_memory(addr + coord_off + 16, &z, sizeof(z))) break;
                
                std::cout << "Канд " << i+1 << " [0x" << std::hex << addr << std::dec << "]: ";
                std::cout << std::fixed << std::setprecision(2);
                std::cout << "(" << x << ", " << y << ", " << z << ")\n";
                break;
            }
        }
        
        std::cout << "\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    return 0;
}
