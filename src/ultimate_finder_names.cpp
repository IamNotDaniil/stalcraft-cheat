#include "memory.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <cmath>
#include <thread>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <cstring>
#include <string>

struct PlayerData {
    uintptr_t addr;
    int health;
    double x, y, z;
    int health_off;
    int coord_off;
    std::string possible_name;
};

bool is_printable(const std::string& str) {
    for (char c : str) {
        if (c < 32 || c > 126) return false;
    }
    return true;
}

int main() {
    std::cout << "=== ULTIMATE STALCRAFT X OFFSET + NAMES FINDER ===\n";
    std::cout << "Встань рядом с игроком, у которого видно ник.\n\n";
    
    while (!memory::init("stalcraft")) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    std::cout << "PID: " << memory::get_pid() << "\n";
    
    const uintptr_t HEAP_START = 0x55556c818000;
    const uintptr_t HEAP_END = 0x55556ec82000;
    
    std::vector<PlayerData> candidates;
    
    std::cout << "Сканируем память в поисках игроков...\n";
    
    int total_scanned = 0;
    for (uintptr_t addr = HEAP_START; addr < HEAP_END - 256; addr += 8) {
        total_scanned++;
        if (total_scanned % 100000 == 0) {
            std::cout << "  Прогресс: " << total_scanned << " адресов\r" << std::flush;
        }
        
        // Используем найденные оффсеты
        int health;
        if (!memory::read_memory(addr + 0x2c, &health, sizeof(health))) continue;
        if (health < 1 || health > 100) continue;
        
        double x, y, z;
        if (!memory::read_memory(addr + 0x0, &x, sizeof(x))) continue;
        if (!memory::read_memory(addr + 0x8, &y, sizeof(y))) continue;
        if (!memory::read_memory(addr + 0x10, &z, sizeof(z))) continue;
        
        if (std::isnan(x) || std::isnan(y) || std::isnan(z)) continue;
        if (std::abs(x) > 10000 || std::abs(y) > 10000 || std::abs(z) > 10000) continue;
        
        PlayerData pd;
        pd.addr = addr;
        pd.health = health;
        pd.x = x; pd.y = y; pd.z = z;
        pd.health_off = 0x2c;
        pd.coord_off = 0x0;
        
        // Ищем имя где-то рядом (в пределах 0x200 байт)
        for (int name_off = 0x40; name_off < 0x200; name_off += 4) {
            char name_buffer[64] = {0};
            if (!memory::read_memory(addr + name_off, name_buffer, 32)) continue;
            
            std::string name(name_buffer);
            
            // Проверяем, похоже ли на имя игрока
            if (name.length() >= 3 && name.length() <= 20 && is_printable(name)) {
                bool has_lower = false, has_upper = false;
                for (char c : name) {
                    if (islower(c)) has_lower = true;
                    if (isupper(c)) has_upper = true;
                }
                
                if (has_lower && has_upper) {
                    pd.possible_name = name;
                    break;
                }
            }
        }
        
        candidates.push_back(pd);
        if (candidates.size() % 100 == 0) {
            std::cout << "Найдено кандидатов: " << candidates.size() << "\r" << std::flush;
        }
    }
    
    std::cout << "\n\n=== СТАТИСТИКА ===\n";
    std::cout << "Всего кандидатов: " << candidates.size() << "\n\n";
    
    // Группируем по координатам
    std::map<std::string, std::vector<PlayerData>> by_position;
    for (auto& pd : candidates) {
        char key[64];
        snprintf(key, sizeof(key), "%.1f,%.1f,%.1f", pd.x, pd.y, pd.z);
        by_position[key].push_back(pd);
    }
    
    std::cout << "Уникальных позиций: " << by_position.size() << "\n\n";
    
    // Ищем объекты с именами
    std::cout << "Объекты с возможными именами:\n";
    int name_count = 0;
    for (auto& [pos, vec] : by_position) {
        for (auto& pd : vec) {
            if (!pd.possible_name.empty()) {
                std::cout << "Адрес: 0x" << std::hex << pd.addr << std::dec << "\n";
                std::cout << "  Позиция: (" << pd.x << ", " << pd.y << ", " << pd.z << ")\n";
                std::cout << "  HP: " << pd.health << "\n";
                std::cout << "  Имя: \"" << pd.possible_name << "\"\n";
                std::cout << "  Смещение имени: 0x" << std::hex << (pd.possible_name.empty() ? 0 : 
                    (uintptr_t)(pd.possible_name.c_str()) - pd.addr) << std::dec << "\n\n";
                name_count++;
                break;
            }
        }
    }
    
    std::cout << "Всего объектов с именами: " << name_count << "\n";
    
    if (name_count > 0) {
        std::cout << "\n✅ Найдены имена! Скорее всего оффсет имени около 0x80-0x100\n";
    } else {
        std::cout << "\n❌ Имена не найдены. Возможно, они хранятся в другом месте.\n";
    }
    
    return 0;
}
