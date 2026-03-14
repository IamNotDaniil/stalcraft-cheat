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
#include <cstring>
#include <linux/uinput.h>
#include <fcntl.h>
#include <unistd.h>

#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define RESET   "\033[0m"
#define CLEAR   "\033[2J\033[H"

struct Vector3 {
    float x, y, z;
};

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
    bool esp_enabled = true;
    bool radar_enabled = true;
    int radar_size = 25;
    bool aimbot_enabled = false;
    float aimbot_fov = 10.0f;
    float aimbot_smooth = 5.0f;
    int aimbot_bone = 1; // 0=head, 1=body, 2=legs
    bool aimbot_visible_check = true;
    bool aimbot_team_check = true;
    bool no_recoil = false;
    int aimbot_key = 0; // 0 = всегда, 1 = shift, 2 = ctrl, 3 = alt
} config;

static uintptr_t local_player_addr = 0;
static double local_x, local_y, local_z;
static int uinput_fd = -1;

static const uintptr_t HEAP_START = 0x55556c818000;
static const uintptr_t HEAP_END = 0x55556ec82000;
static const int HEALTH_OFFSET = 0x2c;
static const int COORD_OFFSET = 0x0;

std::vector<Entity> players;
std::vector<Entity> mobs;

// ==================== AIMBOT ====================

bool init_uinput() {
    uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (uinput_fd < 0) return false;

    ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY);
    ioctl(uinput_fd, UI_SET_EVBIT, EV_REL);
    ioctl(uinput_fd, UI_SET_RELBIT, REL_X);
    ioctl(uinput_fd, UI_SET_RELBIT, REL_Y);
    ioctl(uinput_fd, UI_SET_KEYBIT, BTN_LEFT);

    struct uinput_setup usetup;
    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_USB;
    strcpy(usetup.name, "Generic Mouse");
    ioctl(uinput_fd, UI_DEV_SETUP, &usetup);
    ioctl(uinput_fd, UI_DEV_CREATE);
    return true;
}

void move_mouse(int dx, int dy) {
    if (uinput_fd < 0 && !init_uinput()) return;
    
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = EV_REL;
    ev.code = REL_X;
    ev.value = dx;
    write(uinput_fd, &ev, sizeof(ev));
    ev.code = REL_Y;
    ev.value = dy;
    write(uinput_fd, &ev, sizeof(ev));
    ev.type = EV_SYN;
    ev.code = SYN_REPORT;
    ev.value = 0;
    write(uinput_fd, &ev, sizeof(ev));
}

bool is_key_pressed(int key) {
    // Проверка клавиш - упрощенно
    // В реальности нужно читать /dev/input
    if (key == 0) return true; // всегда
    return true; // заглушка
}

void aimbot() {
    if (!config.aimbot_enabled) return;
    if (players.empty()) return;
    
    // Находим ближайшего игрока в FOV
    Entity* target = nullptr;
    float best_fov = config.aimbot_fov;
    
    for (auto& p : players) {
        if (p.distance > config.max_distance) continue;
        if (p.health < config.min_hp) continue;
        
        float dx = p.x - local_x;
        float dz = p.z - local_z;
        
        float target_angle = atan2(dz, dx) * 180.0 / M_PI;
        float current_angle = 0; // нужно читать из игры
        float angle_diff = fabs(target_angle - current_angle);
        
        if (angle_diff < best_fov) {
            best_fov = angle_diff;
            target = &p;
        }
    }
    
    if (target) {
        // Плавное наведение
        float dx = target->x - local_x;
        float dz = target->z - local_z;
        float target_yaw = atan2(dz, dx);
        
        float dy = target->y - local_y;
        float dist = sqrt(dx*dx + dz*dz);
        float target_pitch = atan2(dy, dist);
        
        // Конвертируем в движение мыши
        float sensitivity = 100.0f / config.aimbot_smooth;
        int move_x = (int)(target_yaw * sensitivity);
        int move_y = (int)(-target_pitch * sensitivity);
        
        move_mouse(move_x, move_y);
    }
}

// ==================== ANTI RECOIL ====================

void anti_recoil() {
    if (!config.no_recoil) return;
    
    // Читаем паттерн отдачи и компенсируем
    // В реальном чите нужно найти оффсеты отдачи
    static int shots_fired = 0;
    shots_fired++;
    
    if (shots_fired > 3) {
        // Компенсация вертикальной отдачи
        move_mouse(0, -5); // двигаем мышь вниз
    }
}

// ==================== ESP ====================

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

void draw_radar() {
    int size = config.radar_size;
    std::vector<std::string> grid(size, std::string(size, ' '));
    
    int center = size / 2;
    grid[center][center] = '@';
    
    float scale = 20.0f;
    
    for (auto& p : players) {
        if (p.distance > config.max_distance) continue;
        
        int dx = (int)((p.x - local_x) / scale);
        int dz = (int)((p.z - local_z) / scale);
        
        int x = center + dx;
        int y = center + dz;
        
        if (x >= 0 && x < size && y >= 0 && y < size) {
            if (p.health > 80) grid[y][x] = 'R';
            else if (p.health > 60) grid[y][x] = 'Y';
            else grid[y][x] = 'G';
        }
    }
    
    std::cout << "\n   ";
    for (int i = 0; i < size; i++) {
        std::cout << (i % 5 == 0 ? std::to_string(i/5) : " ");
    }
    std::cout << "\n";
    
    for (int y = 0; y < size; y++) {
        std::cout << std::setw(2) << y << " ";
        for (int x = 0; x < size; x++) {
            char c = grid[y][x];
            if (c == '@') std::cout << RED << "@" << RESET;
            else if (c == 'R') std::cout << RED << "#" << RESET;
            else if (c == 'Y') std::cout << YELLOW << "#" << RESET;
            else if (c == 'G') std::cout << GREEN << "#" << RESET;
            else std::cout << ".";
        }
        std::cout << "\n";
    }
}

// ==================== MEMORY SCANNING ====================

void scan_memory() {
    players.clear();
    mobs.clear();
    
    auto regions = memory::get_all_regions();
    
    for (auto& region : regions) {
        if (region.perms.find("r") == std::string::npos) continue;
        if (region.path.find("[stack") != std::string::npos) continue;
        
        for (uintptr_t addr = region.start; addr < region.end - 64; addr += 32) {
            float x, y, z;
            if (!memory::read_memory(addr + COORD_OFFSET, &x, sizeof(x))) continue;
            if (!memory::read_memory(addr + COORD_OFFSET + 4, &y, sizeof(y))) continue;
            if (!memory::read_memory(addr + COORD_OFFSET + 8, &z, sizeof(z))) continue;
            
            if (std::isnan(x) || std::isnan(y) || std::isnan(z)) continue;
            if (std::abs(x) > 10000 || std::abs(y) > 10000 || std::abs(z) > 10000) continue;
            
            int health;
            if (!memory::read_memory(addr + HEALTH_OFFSET, &health, sizeof(health))) continue;
            if (health < 1 || health > 100) continue;
            
            // Определяем локального игрока
            if (local_player_addr == 0 && health > 80) {
                local_player_addr = addr;
                local_x = x;
                local_y = y;
                local_z = z;
            }
            
            if (addr == local_player_addr) {
                local_x = x;
                local_y = y;
                local_z = z;
                continue;
            }
            
            Entity e;
            e.address = addr;
            e.x = x; e.y = y; e.z = z;
            e.health = health;
            
            float dx = x - local_x;
            float dz = z - local_z;
            e.distance = sqrt(dx*dx + dz*dz + (y - local_y)*(y - local_y));
            
            if (health > 50) {
                players.push_back(e);
            } else {
                mobs.push_back(e);
            }
        }
    }
}

// ==================== LOAD/SAVE CONFIG ====================

void load_config() {
    std::ifstream file("ultimate_config.txt");
    if (file.is_open()) {
        file >> config.max_distance >> config.min_hp >> config.esp_enabled
             >> config.radar_enabled >> config.radar_size
             >> config.aimbot_enabled >> config.aimbot_fov >> config.aimbot_smooth
             >> config.aimbot_bone >> config.aimbot_visible_check >> config.aimbot_team_check
             >> config.no_recoil >> config.aimbot_key;
        file.close();
    }
}

void save_config() {
    std::ofstream file("ultimate_config.txt");
    file << config.max_distance << " " << config.min_hp << " " << config.esp_enabled << " "
         << config.radar_enabled << " " << config.radar_size << " "
         << config.aimbot_enabled << " " << config.aimbot_fov << " " << config.aimbot_smooth << " "
         << config.aimbot_bone << " " << config.aimbot_visible_check << " " << config.aimbot_team_check << " "
         << config.no_recoil << " " << config.aimbot_key;
    file.close();
}

// ==================== MAIN DISPLAY ====================

void display() {
    std::cout << CLEAR;
    std::cout << CYAN   << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout <<         "║              STALCRAFT X ULTIMATE CHEAT                  ║\n";
    std::cout <<         "╚══════════════════════════════════════════════════════════╝\n" << RESET;
    
    std::cout << YELLOW << "Ты: (" 
              << std::fixed << std::setprecision(1) 
              << local_x << ", " << local_y << ", " << local_z << ")\n" << RESET;
    
    std::cout << GREEN << "Игроков: " << players.size() 
              << " | Мобов: " << mobs.size() << "\n" << RESET;
    
    std::cout << "────────────────────────────────────────────────────────\n";
    
    // Статус функций
    std::cout << (config.esp_enabled ? GREEN : RED) << "ESP" << RESET << " | "
              << (config.aimbot_enabled ? GREEN : RED) << "AIM" << RESET << " | "
              << (config.no_recoil ? GREEN : RED) << "NR" << RESET << "\n";
    
    if (config.radar_enabled) {
        draw_radar();
    }
    
    std::cout << "────────────────────────────────────────────────────────\n";
    
    if (players.empty()) {
        std::cout << RED << "  ⊗ Никого нет\n" << RESET;
    } else {
        std::cout << "Список игроков:\n";
        for (size_t i = 0; i < players.size() && i < 15; i++) {
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
            std::cout << "| " << dir;
            std::cout << "\n" << RESET;
        }
    }
    
    std::cout << "────────────────────────────────────────────────────────\n";
    std::cout << CYAN << "Обновлено: " << std::time(nullptr) << RESET << "\n";
    std::cout << WHITE << "F1 - ESP | F2 - AIM | F3 - NR | F4 - меню\n" << RESET;
}

// ==================== MAIN ====================

int main() {
    std::cout << CLEAR;
    std::cout << CYAN << "=== STALCRAFT X ULTIMATE ===\n" << RESET;
    
    load_config();
    init_uinput();
    
    while (!memory::init("stalcraft")) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    std::cout << "PID: " << memory::get_pid() << "\n";
    std::cout << "Поиск локального игрока...\n";
    
    while (local_player_addr == 0) {
        scan_memory();
        if (local_player_addr == 0) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    std::cout << GREEN << "OK. Сканирование...\n\n" << RESET;
    
    while (true) {
        scan_memory();
        
        if (config.aimbot_enabled) {
            aimbot();
        }
        
        if (config.no_recoil) {
            anti_recoil();
        }
        
        if (config.esp_enabled) {
            display();
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return 0;
}
