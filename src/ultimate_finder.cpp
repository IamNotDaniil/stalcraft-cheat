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

struct PlayerData {
    uintptr_t addr;
    int health;
    double x, y, z;
    int health_off;
    int coord_off;
};

int main() {
    std::cout << "=== ULTIMATE STALCRAFT X OFFSET FINDER ===\n";
    std::cout << "Встань в толпу из 20+ игроков и запусти это.\n\n";
    
    while (!memory::init("stalcraft")) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    std::cout << "PID: " << memory::get_pid() << "\n";
    
    const uintptr_t HEAP_START = 0x55556c818000;
    const uintptr_t HEAP_END = 0x55556ec82000;
    
    std::map<uintptr_t, std::vector<PlayerData>> candidates_by_addr;
    
    std::cout << "Сканируем память...\n";
    
    int total_scanned = 0;
    for (uintptr_t addr = HEAP_START; addr < HEAP_END - 128; addr += 8) {
        total_scanned++;
        if (total_scanned % 100000 == 0) {
            std::cout << "  Прогресс: " << total_scanned << " адресов\r" << std::flush;
        }
        
        for (int health_off = 0; health_off < 0x80; health_off += 4) {
            int health;
            if (!memory::read_memory(addr + health_off, &health, sizeof(health))) continue;
            if (health < 1 || health > 100) continue;
            
            for (int coord_off = 0; coord_off < 0x80; coord_off += 8) {
                double x, y, z;
                if (!memory::read_memory(addr + coord_off, &x, sizeof(x))) continue;
                if (!memory::read_memory(addr + coord_off + 8, &y, sizeof(y))) continue;
                if (!memory::read_memory(addr + coord_off + 16, &z, sizeof(z))) continue;
                
                if (std::isnan(x) || std::isnan(y) || std::isnan(z)) continue;
                if (std::abs(x) > 10000 || std::abs(y) > 10000 || std::abs(z) > 10000) continue;
                if (std::abs(x) < 0.1 && std::abs(y) < 0.1 && std::abs(z) < 0.1) continue;
                
                PlayerData pd;
                pd.addr = addr;
                pd.health = health;
                pd.x = x; pd.y = y; pd.z = z;
                pd.health_off = health_off;
                pd.coord_off = coord_off;
                
                candidates_by_addr[addr].push_back(pd);
                break;
            }
        }
    }
    
    std::cout << "\n\nНайдено уникальных адресов: " << candidates_by_addr.size() << "\n\n";
    
    // Анализируем, какие смещения чаще всего встречаются вместе
    std::map<std::pair<int, int>, int> offset_pairs;
    
    for (auto& [addr, candidates] : candidates_by_addr) {
        if (candidates.size() > 10) continue; // слишком много полей — мусор
        
        for (auto& pd : candidates) {
            offset_pairs[{pd.health_off, pd.coord_off}]++;
        }
    }
    
    std::vector<std::pair<std::pair<int, int>, int>> pairs_vec(offset_pairs.begin(), offset_pairs.end());
    std::sort(pairs_vec.begin(), pairs_vec.end(),
        [](auto& a, auto& b) { return a.second > b.second; });
    
    std::cout << "Топ связок (смещение_здоровья, смещение_координат):\n";
    for (int i = 0; i < std::min(10, (int)pairs_vec.size()); i++) {
        auto [offsets, count] = pairs_vec[i];
        std::cout << "  health=0x" << std::hex << offsets.first 
                  << ", coords=0x" << offsets.second << std::dec 
                  << " : " << count << " объектов\n";
    }
    
    // Берём топ-связку
    if (!pairs_vec.empty()) {
        auto best = pairs_vec[0].first;
        int best_health_off = best.first;
        int best_coord_off = best.second;
        
        std::cout << "\n✅ ЛУЧШИЕ ОФФСЕТЫ:\n";
        std::cout << "  health_offset = 0x" << std::hex << best_health_off << "\n";
        std::cout << "  coord_offset  = 0x" << best_coord_off << "\n\n";
        
        // Показываем примеры объектов с этими оффсетами
        std::cout << "Примеры игроков с этими оффсетами:\n";
        int examples = 0;
        
        for (auto& [addr, candidates] : candidates_by_addr) {
            for (auto& pd : candidates) {
                if (pd.health_off == best_health_off && pd.coord_off == best_coord_off) {
                    std::cout << "  Адрес: 0x" << std::hex << addr << std::dec 
                              << " | HP: " << pd.health 
                              << " | Позиция: (" << std::fixed << std::setprecision(1) 
                              << pd.x << ", " << pd.y << ", " << pd.z << ")\n";
                    examples++;
                    if (examples >= 10) break;
                }
            }
            if (examples >= 10) break;
        }
        
        // Сохраняем в файл
        std::ofstream file("offsets.txt");
        if (file.is_open()) {
            file << "health_offset=0x" << std::hex << best_health_off << "\n";
            file << "coord_offset=0x" << best_coord_off << "\n";
            file.close();
            std::cout << "\n💾 Оффсеты сохранены в offsets.txt\n";
        } else {
            std::cout << "\n❌ Не удалось сохранить файл\n";
        }
    }
    
    return 0;
}
