#include "memory.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <thread>
#include <chrono>
#include <algorithm>
#include <map>
#include <set>

struct Entity {
    uintptr_t address;
    double x, y, z;
    int health;
};

static uintptr_t local_player_addr = 0;
static double local_x, local_y, local_z;

// Адреса анонимных регионов
static const uintptr_t HEAP_START = 0x55556c818000;
static const uintptr_t HEAP_END = 0x55556ec82000;

// Оффсеты
static const int HEALTH_OFFSET = 0x2c;
static const int COORD_OFFSET = 0x30;  // <-- найденный оффсет

void find_all_entities(std::vector<Entity>& entities) {
    std::map<uintptr_t, Entity> unique_entities;
    
    for (uintptr_t addr = HEAP_START; addr < HEAP_END - 64; addr += 32) {
        
        // Читаем координаты по найденному смещению
        double x, y, z;
        if (!memory::read_memory(addr + COORD_OFFSET, &x, sizeof(x))) continue;
        if (!memory::read_memory(addr + COORD_OFFSET + 8, &y, sizeof(y))) continue;
        if (!memory::read_memory(addr + COORD_OFFSET + 16, &z, sizeof(z))) continue;
        
        if (std::isnan(x) || std::isnan(y) || std::isnan(z)) continue;
        if (std::abs(x) > 10000 || std::abs(y) > 10000 || std::abs(z) > 10000) continue;
        if (std::abs(x) < 0.001 && std::abs(y) < 0.001 && std::abs(z) < 0.001) continue;
        
        // Читаем здоровье
        int health;
        if (!memory::read_memory(addr + HEALTH_OFFSET, &health, sizeof(health))) continue;
        if (health < 1 || health > 100) continue;
        
        Entity e;
        e.address = addr;
        e.x = x; e.y = y; e.z = z;
        e.health = health;
        
        unique_entities[addr] = e;
    }
    
    entities.clear();
    for (auto& [addr, e] : unique_entities) {
        entities.push_back(e);
    }
}

void find_local_player() {
    std::vector<Entity> entities;
    find_all_entities(entities);
    
    if (entities.empty()) return;
    
    // Локальный игрок — самый здоровый
    auto& best = *std::max_element(entities.begin(), entities.end(),
        [](const Entity& a, const Entity& b) { return a.health < b.health; });
    
    local_player_addr = best.address;
    local_x = best.x; local_y = best.y; local_z = best.z;
    
    std::cout << "Найден локальный игрок: HP=" << best.health << "\n";
}

void radar() {
    if (local_player_addr == 0) return;
    
    std::vector<Entity> entities;
    find_all_entities(entities);
    
    // Обновляем позицию локального игрока
    for (auto& e : entities) {
        if (e.address == local_player_addr) {
            local_x = e.x; local_y = e.y; local_z = e.z;
            break;
        }
    }
    
    // Другие игроки (HP > 50)
    std::vector<Entity> others;
    for (auto& e : entities) {
        if (e.address != local_player_addr && e.health > 50) {
            others.push_back(e);
        }
    }
    
    std::sort(others.begin(), others.end(), [&](const Entity& a, const Entity& b) {
        double da = (a.x - local_x)*(a.x - local_x) + (a.y - local_y)*(a.y - local_y) + (a.z - local_z)*(a.z - local_z);
        double db = (b.x - local_x)*(b.x - local_x) + (b.y - local_y)*(b.y - local_y) + (b.z - local_z)*(b.z - local_z);
        return da < db;
    });
    
    std::cout << "\033[2J\033[H";
    std::cout << "=== STALCRAFT X RADAR ===\n";
    std::cout << "Ты: (" << std::fixed << std::setprecision(1) 
              << local_x << ", " << local_y << ", " << local_z << ")\n";
    std::cout << "Игроков рядом: " << others.size() << "\n";
    std::cout << "----------------------------------------\n";
    
    if (others.empty()) {
        std::cout << "Никого нет. Ты один.\n";
    } else {
        for (size_t i = 0; i < others.size() && i < 20; i++) {
            double dist = sqrt((others[i].x - local_x)*(others[i].x - local_x) + 
                              (others[i].y - local_y)*(others[i].y - local_y) + 
                              (others[i].z - local_z)*(others[i].z - local_z));
            
            std::cout << std::setw(2) << i+1 << ". ";
            std::cout << "Дист: " << std::setw(5) << std::fixed << std::setprecision(1) << dist << "м";
            std::cout << " | HP: " << std::setw(3) << others[i].health;
            std::cout << "\n";
        }
    }
}

int main() {
    std::cout << "=== STALCRAFT X RADAR ===\n";
    
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
    
    std::cout << "OK. Сканирование...\n\n";
    
    while (true) {
        radar();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    return 0;
}
