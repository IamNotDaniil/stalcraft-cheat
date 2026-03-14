#include "memory.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <thread>
#include <chrono>
#include <algorithm>
#include <map>

struct Vector3 {
    float x, y, z;
};

struct Player {
    uintptr_t address;
    double x, y, z;
    int health;
    int team;
    bool is_player;  // true = игрок, false = моб
};

static uintptr_t local_player_addr = 0;

// Оффсеты
static const uintptr_t HEALTH_OFFSET = 0x2c;
static uintptr_t TEAM_OFFSET = 0x3c;  // будем уточнять
static const uintptr_t ANGLE_OFFSET = 0x18;

// Смещения координат (подобрано)
static const uintptr_t COORD_OFFSET = 0x20;  // из предыдущего запуска

void find_local_player() {
    auto regions = memory::get_all_regions();
    
    for (auto& region : regions) {
        if (region.perms.find("r") == std::string::npos) continue;
        if (region.path.find("[stack") != std::string::npos) continue;
        
        for (uintptr_t addr = region.start; addr < region.end - 64; addr += 16) {
            double x, y, z;
            if (!memory::read_memory(addr + COORD_OFFSET, &x, sizeof(x))) continue;
            if (!memory::read_memory(addr + COORD_OFFSET + 8, &y, sizeof(y))) continue;
            if (!memory::read_memory(addr + COORD_OFFSET + 16, &z, sizeof(z))) continue;
            
            if (std::isnan(x) || std::isnan(y) || std::isnan(z)) continue;
            if (std::abs(x) > 10000 || std::abs(y) > 10000 || std::abs(z) > 10000) continue;
            if (std::abs(x) < 0.1 && std::abs(y) < 0.1 && std::abs(z) < 0.1) continue;
            
            int health;
            if (!memory::read_memory(addr + HEALTH_OFFSET, &health, sizeof(health))) continue;
            if (health < 1 || health > 100) continue;
            
            local_player_addr = addr;
            std::cout << "Локальный игрок: 0x" << std::hex << addr << std::dec << "\n";
            return;
        }
    }
}

float distance(double x1, double y1, double z1, double x2, double y2, double z2) {
    double dx = x1 - x2;
    double dy = y1 - y2;
    double dz = z1 - z2;
    return sqrt(dx*dx + dy*dy + dz*dz);
}

void scan_players() {
    if (local_player_addr == 0) return;
    
    std::vector<Player> players;
    std::map<int, int> team_stats;  // для анализа оффсета команды
    
    auto regions = memory::get_all_regions();
    for (auto& region : regions) {
        if (region.perms.find("r") == std::string::npos) continue;
        if (region.path.find("[stack") != std::string::npos) continue;
        
        for (uintptr_t addr = region.start; addr < region.end - 64; addr += 16) {
            if (addr == local_player_addr) continue;
            
            double x, y, z;
            if (!memory::read_memory(addr + COORD_OFFSET, &x, sizeof(x))) continue;
            if (!memory::read_memory(addr + COORD_OFFSET + 8, &y, sizeof(y))) continue;
            if (!memory::read_memory(addr + COORD_OFFSET + 16, &z, sizeof(z))) continue;
            
            if (std::isnan(x) || std::isnan(y) || std::isnan(z)) continue;
            if (std::abs(x) > 10000 || std::abs(y) > 10000 || std::abs(z) > 10000) continue;
            
            int health;
            if (!memory::read_memory(addr + HEALTH_OFFSET, &health, sizeof(health))) continue;
            if (health < 1 || health > 100) continue;
            
            // Пробуем разные оффсеты для команды
            int team_values[5];
            for (int off = 0x30; off <= 0x50; off += 4) {
                memory::read_memory(addr + off, &team_values[(off-0x30)/4], sizeof(int));
            }
            
            Player p;
            p.address = addr;
            p.x = x; p.y = y; p.z = z;
            p.health = health;
            p.team = team_values[3];  // 0x3c
            
            // Собираем статистику по командам
            for (int v : team_values) {
                team_stats[v]++;
            }
            
            // Игроки обычно имеют команду 0 или 1
            // Мобы имеют мусорные значения
            p.is_player = (p.team == 0 || p.team == 1);
            
            players.push_back(p);
            if (players.size() >= 32) break;
        }
        if (players.size() >= 32) break;
    }
    
    // Координаты локального игрока
    double lx=0, ly=0, lz=0;
    memory::read_memory(local_player_addr + COORD_OFFSET, &lx, sizeof(lx));
    memory::read_memory(local_player_addr + COORD_OFFSET + 8, &ly, sizeof(ly));
    memory::read_memory(local_player_addr + COORD_OFFSET + 16, &lz, sizeof(lz));
    
    // Сортировка по расстоянию
    std::sort(players.begin(), players.end(), [&](const Player& a, const Player& b) {
        return distance(lx, ly, lz, a.x, a.y, a.z) < distance(lx, ly, lz, b.x, b.y, b.z);
    });
    
    // Очистка экрана
    std::cout << "\033[2J\033[H";
    
    std::cout << "=== STALCRAFT X Radar ===\n";
    std::cout << "Локальная позиция: (" << std::fixed << std::setprecision(1) 
              << lx << ", " << ly << ", " << lz << ")\n";
    std::cout << "----------------------------------------\n";
    
    int player_count = 0, mob_count = 0;
    for (size_t i = 0; i < players.size() && i < 15; i++) {
        float dist = distance(lx, ly, lz, players[i].x, players[i].y, players[i].z);
        
        if (players[i].is_player) player_count++;
        else mob_count++;
        
        std::cout << std::setw(2) << i+1 << ". ";
        std::cout << (players[i].is_player ? "👤 " : "👾 ");
        std::cout << "Дист: " << std::setw(6) << std::fixed << std::setprecision(1) << dist << "м";
        std::cout << " | HP: " << std::setw(3) << players[i].health;
        std::cout << " | Команда: " << std::setw(4) << players[i].team;
        if (!players[i].is_player) {
            std::cout << " [МОБ]";
        }
        std::cout << "\n";
    }
    
    std::cout << "----------------------------------------\n";
    std::cout << "Итого: 👤 " << player_count << " игроков, 👾 " << mob_count << " мобов\n";
    
    // Выводим статистику команд для анализа
    std::cout << "\nСтатистика команд (для настройки оффсета):\n";
    int i = 0;
    for (auto& [val, cnt] : team_stats) {
        if (i++ > 10) break;
        std::cout << "  0x" << std::hex << val << std::dec << " : " << cnt << "\n";
    }
}

int main() {
    std::cout << "=== STALCRAFT X Radar ===\n";
    
    while (!memory::init("stalcraft")) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    std::cout << "Игра найдена, PID: " << memory::get_pid() << "\n";
    std::cout << "Поиск локального игрока...\n";
    
    while (local_player_addr == 0) {
        find_local_player();
        if (local_player_addr == 0) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    std::cout << "Сканирование...\n\n";
    
    while (true) {
        scan_players();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    return 0;
}
