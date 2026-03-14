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
#include <ctime>
#include <fstream>

#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define RESET   "\033[0m"
#define CLEAR   "\033[2J\033[H"

struct Entity {
    uintptr_t address;
    double x, y, z;
    int health;
    double distance;
    double angle;
};

struct Config {
    int max_distance = 500;
    int min_hp = 50;
    bool show_radar = true;
    int radar_size = 30;
} config;

static uintptr_t local_player_addr = 0;
static double local_x, local_y, local_z;

static const uintptr_t HEAP_START = 0x55556c818000;
static const uintptr_t HEAP_END = 0x55556ec82000;
static const int HEALTH_OFFSET = 0x2c;
static const int COORD_OFFSET = 0x0;

void load_config() {
    std::ifstream file("radar_config.txt");
    if (file.is_open()) {
        file >> config.max_distance >> config.min_hp >> config.show_radar >> config.radar_size;
        file.close();
    }
}

void save_config() {
    std::ofstream file("radar_config.txt");
    if (file.is_open()) {
        file << config.max_distance << " " << config.min_hp << " " 
             << config.show_radar << " " << config.radar_size;
        file.close();
    }
}

std::string get_direction(float angle) {
    if (angle >= -22.5 && angle < 22.5) return "→ Впереди";
    if (angle >= 22.5 && angle < 67.5) return "↗ Спереди-справа";
    if (angle >= 67.5 && angle < 112.5) return "↑ Справа";
    if (angle >= 112.5 && angle < 157.5) return "↖ Сзади-справа";
    if (angle >= 157.5 || angle < -157.5) return "← Сзади";
    if (angle >= -157.5 && angle < -112.5) return "↙ Сзади-слева";
    if (angle >= -112.5 && angle < -67.5) return "↓ Слева";
    if (angle >= -67.5 && angle < -22.5) return "↘ Спереди-слева";
    return "• Неизвестно";
}

void draw_radar(const std::vector<Entity>& players) {
    int size = config.radar_size;
    std::vector<std::string> grid(size, std::string(size, '.'));
    
    int center = size / 2;
    grid[center][center] = 'X';
    
    float scale = 10.0f;
    
    for (auto& p : players) {
        if (p.distance > config.max_distance) continue;
        
        int dx = (int)((p.x - local_x) / scale);
        int dz = (int)((p.z - local_z) / scale);
        
        int x = center + dx;
        int y = center + dz;
        
        if (x >= 0 && x < size && y >= 0 && y < size) {
            if (grid[y][x] == '.') {
                if (p.health > 80) grid[y][x] = 'R';
                else if (p.health > 60) grid[y][x] = 'Y';
                else grid[y][x] = 'G';
            }
        }
    }
    
    std::cout << "\n🗺️ Радар (1 клетка = 10м):\n";
    for (auto& row : grid) {
        for (char c : row) {
            if (c == 'X') std::cout << RED << '●' << RESET;
            else if (c == 'R') std::cout << RED << '■' << RESET;
            else if (c == 'Y') std::cout << YELLOW << '■' << RESET;
            else if (c == 'G') std::cout << GREEN << '■' << RESET;
            else std::cout << CYAN << '·' << RESET;
        }
        std::cout << "\n";
    }
}

void find_all_entities(std::vector<Entity>& entities) {
    std::map<uintptr_t, Entity> unique_by_addr;
    std::map<std::string, Entity> unique_by_pos;
    
    for (uintptr_t addr = HEAP_START; addr < HEAP_END - 64; addr += 32) {
        
        double x, y, z;
        if (!memory::read_memory(addr + COORD_OFFSET, &x, sizeof(x))) continue;
        if (!memory::read_memory(addr + COORD_OFFSET + 8, &y, sizeof(y))) continue;
        if (!memory::read_memory(addr + COORD_OFFSET + 16, &z, sizeof(z))) continue;
        
        if (std::isnan(x) || std::isnan(y) || std::isnan(z)) continue;
        if (std::abs(x) > 10000 || std::abs(y) > 10000 || std::abs(z) > 10000) continue;
        
        int health;
        if (!memory::read_memory(addr + HEALTH_OFFSET, &health, sizeof(health))) continue;
        if (health < 1 || health > 100) continue;
        
        Entity e;
        e.address = addr;
        e.x = x; e.y = y; e.z = z;
        e.health = health;
        
        unique_by_addr[addr] = e;
    }
    
    for (auto& [addr, e] : unique_by_addr) {
        char key[64];
        snprintf(key, sizeof(key), "%.1f,%.1f,%.1f", e.x, e.y, e.z);
        std::string pos_key(key);
        
        if (unique_by_pos.find(pos_key) == unique_by_pos.end()) {
            unique_by_pos[pos_key] = e;
        }
    }
    
    entities.clear();
    for (auto& [pos, e] : unique_by_pos) {
        entities.push_back(e);
    }
}

void find_local_player() {
    std::vector<Entity> entities;
    find_all_entities(entities);
    
    if (entities.empty()) return;
    
    auto& best = *std::max_element(entities.begin(), entities.end(),
        [](const Entity& a, const Entity& b) { return a.health < b.health; });
    
    local_player_addr = best.address;
    local_x = best.x; local_y = best.y; local_z = best.z;
}

void radar() {
    if (local_player_addr == 0) return;
    
    std::vector<Entity> entities;
    find_all_entities(entities);
    
    for (auto& e : entities) {
        if (e.address == local_player_addr) {
            local_x = e.x; local_y = e.y; local_z = e.z;
            break;
        }
    }
    
    std::vector<Entity> players;
    for (auto& e : entities) {
        if (e.address != local_player_addr) {
            
            double dx = e.x - local_x;
            double dz = e.z - local_z;
            
            e.distance = sqrt(dx*dx + dz*dz + (e.y - local_y)*(e.y - local_y));
            
            if (e.distance > 1.0 && e.distance < config.max_distance && 
                e.health > config.min_hp && e.health <= 100) {
                
                double angle_rad = atan2(dz, dx);
                e.angle = angle_rad * 180.0 / M_PI;
                
                players.push_back(e);
            }
        }
    }
    
    std::sort(players.begin(), players.end(),
        [](const Entity& a, const Entity& b) { return a.distance < b.distance; });
    
    std::cout << CLEAR;
    std::cout << CYAN   << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout <<         "║                    STALCRAFT X RADAR                      ║\n";
    std::cout <<         "╚══════════════════════════════════════════════════════════╝\n" << RESET;
    
    std::cout << YELLOW << "Ты: (" 
              << std::fixed << std::setprecision(1) 
              << local_x << ", " << local_y << ", " << local_z << ")\n" << RESET;
    std::cout << GREEN << "Игроков рядом: " << players.size() << "\n" << RESET;
    
    if (config.show_radar && !players.empty()) {
        draw_radar(players);
    }
    
    std::cout << "────────────────────────────────────────────────────────\n";
    
    if (players.empty()) {
        std::cout << RED << "  ⊗ Никого нет. Ты один.\n" << RESET;
    } else {
        for (size_t i = 0; i < players.size() && i < 30; i++) {
            std::string dir = get_direction(players[i].angle);
            
            if (players[i].health > 80) 
                std::cout << RED;
            else if (players[i].health > 60) 
                std::cout << YELLOW;
            else 
                std::cout << GREEN;
            
            std::cout << std::setw(2) << i+1 << ". ";
            std::cout << "Дист: " << std::setw(5) << std::fixed << std::setprecision(1) << players[i].distance << "м ";
            std::cout << "| HP: " << std::setw(3) << players[i].health << " ";
            std::cout << "| " << std::setw(18) << dir << " ";
            std::cout << "| [" << std::setw(5) << (int)players[i].angle << "°]";
            std::cout << "\n" << RESET;
        }
    }
    
    std::cout << "────────────────────────────────────────────────────────\n";
    std::cout << CYAN << "Обновлено: " << std::time(nullptr) << RESET << "\n";
}

int main() {
    std::cout << CLEAR;
    std::cout << CYAN << "=== STALCRAFT X RADAR [FINAL] ===\n" << RESET;
    std::cout << "Оффсеты: health=0x2c, coords=0x0\n\n";
    
    load_config();
    
    while (!memory::init("stalcraft")) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    std::cout << "PID: " << memory::get_pid() << "\n";
    std::cout << "Поиск локального игрока...\n";
    
    while (local_player_addr == 0) {
        find_local_player();
        if (local_player_addr == 0) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    std::cout << GREEN << "OK. Сканирование...\n\n" << RESET;
    
    while (true) {
        radar();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    return 0;
}
