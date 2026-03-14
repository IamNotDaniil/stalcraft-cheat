#include "aimbot.h"
#include "memory.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <thread>
#include <chrono>
#include <linux/uinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <random>
#include <vector>
#include <map>
#include <set>

namespace aimbot {
    Config g_config;
    static std::vector<Player> players;
    static int uinput_fd = -1;
    static uintptr_t g_local_player_ptr_addr = 0;
    static uintptr_t g_entity_list_addr = 0;

    static uintptr_t g_health_offset = 0x2c;
    static uintptr_t g_team_offset = 0x3c;
    static uintptr_t g_visible_offset = 0;
    static uintptr_t g_head_offset = 0x30;
    static uintptr_t g_body_offset = 0x0;
    static uintptr_t g_legs_offset = 0x48;
    static uintptr_t g_view_angles_offset = 0x18;
    static uintptr_t g_shoot_offset = 0;
    static uintptr_t g_recoil_offset = 0;

    static std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
    static uintptr_t local_player_addr = 0;
    static int64_t last_player_print = 0;

    void trigger_check();

    bool init_uinput() {
        uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        if (uinput_fd < 0) return false;

        ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY);
        ioctl(uinput_fd, UI_SET_EVBIT, EV_REL);
        ioctl(uinput_fd, UI_SET_RELBIT, REL_X);
        ioctl(uinput_fd, UI_SET_RELBIT, REL_Y);
        ioctl(uinput_fd, UI_SET_KEYBIT, BTN_LEFT);
        ioctl(uinput_fd, UI_SET_KEYBIT, BTN_RIGHT);

        struct uinput_setup usetup;
        memset(&usetup, 0, sizeof(usetup));
        usetup.id.bustype = BUS_USB;
        usetup.id.vendor = 0x1234;
        usetup.id.product = 0x5678;
        strcpy(usetup.name, "Generic Mouse");
        ioctl(uinput_fd, UI_DEV_SETUP, &usetup);
        ioctl(uinput_fd, UI_DEV_CREATE);
        return true;
    }

    void move_mouse(int dx, int dy) {
        if (uinput_fd < 0 && !init_uinput()) return;
        
        std::uniform_int_distribution<> dist(-1, 1);
        dx += dist(rng);
        dy += dist(rng);
        
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

    void click_mouse(bool down) {
        if (uinput_fd < 0 && !init_uinput()) return;
        
        if (down) {
            std::uniform_int_distribution<> dist(5, 15);
            usleep(dist(rng) * 1000);
        }
        
        struct input_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = EV_KEY;
        ev.code = BTN_LEFT;
        ev.value = down ? 1 : 0;
        write(uinput_fd, &ev, sizeof(ev));
        ev.type = EV_SYN;
        ev.code = SYN_REPORT;
        ev.value = 0;
        write(uinput_fd, &ev, sizeof(ev));
    }

    Vector3 get_bone_pos(const Player& p) {
        switch (g_config.bone) {
            case 0: return p.head;
            case 1: return p.body;
            case 2: return p.legs;
            default: return p.body;
        }
    }

    float calculate_fov(const Vector3& from, const Vector3& to, const Vector3& angles) {
        Vector3 dir = {to.x - from.x, to.y - from.y, to.z - from.z};
        float len = sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
        if (len < 0.001f) return 999;
        dir.x /= len; dir.y /= len; dir.z /= len;

        float yaw = angles.x;
        float pitch = angles.y;
        Vector3 view_dir;
        view_dir.x = cos(pitch) * cos(yaw);
        view_dir.y = cos(pitch) * sin(yaw);
        view_dir.z = sin(pitch);

        float dot = dir.x*view_dir.x + dir.y*view_dir.y + dir.z*view_dir.z;
        dot = std::clamp(dot, -1.0f, 1.0f);
        return acos(dot) * 180.0f / M_PI;
    }

    void find_local_player() {
        std::cout << "🔍 Запуск поиска локального игрока...\n";
        
        auto regions = memory::get_all_regions();
        std::cout << "  Найдено регионов: " << regions.size() << "\n";
        
        int checked = 0;
        int region_num = 0;
        
        for (auto& region : regions) {
            region_num++;
            
            // Пропускаем заведомо бесполезные регионы
            if (region.path.find("[stack") != std::string::npos) continue;
            if (region.path.find("[vsyscall") != std::string::npos) continue;
            if (region.path.find("[vvar") != std::string::npos) continue;
            if (region.path.find("[vdso") != std::string::npos) continue;
            
            // Нужны права на чтение
            if (region.perms.find("r") == std::string::npos) continue;
            
            size_t size = region.end - region.start;
            
            // Пропускаем слишком маленькие
            if (size < 4096) continue;
            
            // Пропускаем слишком большие (больше 50MB) — там точно не игроки
            if (size > 50 * 1024 * 1024) continue;
            
            std::cout << "  Регион " << region_num << "/" << regions.size() 
                      << ": 0x" << std::hex << region.start << " - 0x" << region.end 
                      << " (" << (size/1024) << " KB) " << region.perms << std::dec << "\n";
            
            // Сканируем с шагом 16 байт для скорости
            for (uintptr_t addr = region.start; addr < region.end - 64; addr += 16) {
                checked++;
                
                if (checked % 100000 == 0) {
                    std::cout << "    Проверено адресов: " << checked << "\r" << std::flush;
                }
                
                double x, y, z;
                if (!memory::read_memory(addr + 0, &x, sizeof(x))) continue;
                if (!memory::read_memory(addr + 8, &y, sizeof(y))) continue;
                if (!memory::read_memory(addr + 16, &z, sizeof(z))) continue;
                
                if (std::isnan(x) || std::isnan(y) || std::isnan(z)) continue;
                if (std::abs(x) > 10000 || std::abs(y) > 10000 || std::abs(z) > 10000) continue;
                if (std::abs(x) < 0.1 && std::abs(y) < 0.1 && std::abs(z) < 0.1) continue;
                
                int health;
                if (!memory::read_memory(addr + g_health_offset, &health, sizeof(health))) continue;
                if (health < 1 || health > 100) continue;
                
                float yaw, pitch;
                if (!memory::read_memory(addr + g_view_angles_offset, &yaw, sizeof(yaw))) continue;
                if (!memory::read_memory(addr + g_view_angles_offset + 4, &pitch, sizeof(pitch))) continue;
                
                std::cout << "\n  ✅ Найден игрок: addr=0x" << std::hex << addr 
                          << " pos=(" << x << ", " << y << ", " << z << ")"
                          << " health=" << std::dec << health << "\n";
                
                local_player_addr = addr;
                return;
            }
        }
        
        std::cout << "\n  ❌ Игрок не найден. Проверено адресов: " << checked << "\n";
    }

    void scan_for_players() {
        if (local_player_addr == 0) {
            find_local_player();
            return;
        }

        players.clear();
        
        auto regions = memory::get_all_regions();
        for (auto& region : regions) {
            if (region.path.find("[stack") != std::string::npos) continue;
            if (region.path.find("[vsyscall") != std::string::npos) continue;
            if (region.perms.find("r") == std::string::npos) continue;
            
            size_t size = region.end - region.start;
            if (size < 4096 || size > 50 * 1024 * 1024) continue;
            
            for (uintptr_t addr = region.start; addr < region.end - 64; addr += 16) {
                if (addr == local_player_addr) continue;
                
                double x, y, z;
                if (!memory::read_memory(addr + 0, &x, sizeof(x))) continue;
                if (!memory::read_memory(addr + 8, &y, sizeof(y))) continue;
                if (!memory::read_memory(addr + 16, &z, sizeof(z))) continue;
                
                if (std::isnan(x) || std::isnan(y) || std::isnan(z)) continue;
                if (std::abs(x) > 10000 || std::abs(y) > 10000 || std::abs(z) > 10000) continue;
                if (std::abs(x) < 0.1 && std::abs(y) < 0.1 && std::abs(z) < 0.1) continue;
                
                int health;
                if (!memory::read_memory(addr + g_health_offset, &health, sizeof(health))) continue;
                if (health < 1 || health > 100) continue;
                
                float yaw, pitch;
                if (!memory::read_memory(addr + g_view_angles_offset, &yaw, sizeof(yaw))) continue;
                if (!memory::read_memory(addr + g_view_angles_offset + 4, &pitch, sizeof(pitch))) continue;
                
                Player p;
                p.address = addr;
                p.body = {(float)x, (float)y, (float)z};
                p.health = health;
                
                double hx, hy, hz;
                if (memory::read_memory(addr + g_head_offset, &hx, sizeof(hx)) &&
                    memory::read_memory(addr + g_head_offset + 8, &hy, sizeof(hy)) &&
                    memory::read_memory(addr + g_head_offset + 16, &hz, sizeof(hz))) {
                    p.head = {(float)hx, (float)hy, (float)hz};
                } else {
                    p.head = p.body;
                }
                
                double lx, ly, lz;
                if (memory::read_memory(addr + g_legs_offset, &lx, sizeof(lx)) &&
                    memory::read_memory(addr + g_legs_offset + 8, &ly, sizeof(ly)) &&
                    memory::read_memory(addr + g_legs_offset + 16, &lz, sizeof(lz))) {
                    p.legs = {(float)lx, (float)ly, (float)lz};
                } else {
                    p.legs = p.body;
                }
                
                memory::read_memory(addr + g_team_offset, &p.team, sizeof(p.team));
                
                players.push_back(p);
                
                if (players.size() >= 32) break;
            }
            if (players.size() >= 32) break;
        }
        
        static int last_count = 0;
        if ((int)players.size() != last_count) {
            std::cout << "Найдено игроков: " << players.size() << "\n";
            last_count = players.size();
        }
    }

    void set_offsets(uintptr_t entity_list, uintptr_t local_player_ptr,
                     uintptr_t health_off, uintptr_t team_off, uintptr_t visible_off,
                     uintptr_t head_off, uintptr_t body_off, uintptr_t legs_off,
                     uintptr_t view_angles_off, uintptr_t shoot_off, uintptr_t recoil_off) {
        g_entity_list_addr = entity_list;
        g_local_player_ptr_addr = local_player_ptr;
        
        if (health_off != 0) g_health_offset = health_off;
        if (team_off != 0) g_team_offset = team_off;
        if (visible_off != 0) g_visible_offset = visible_off;
        if (head_off != 0) g_head_offset = head_off;
        if (body_off != 0) g_body_offset = body_off;
        if (legs_off != 0) g_legs_offset = legs_off;
        if (view_angles_off != 0) g_view_angles_offset = view_angles_off;
        if (shoot_off != 0) g_shoot_offset = shoot_off;
        if (recoil_off != 0) g_recoil_offset = recoil_off;
        
        std::cout << "Aimbot offsets set:\n";
        std::cout << "  health: 0x" << std::hex << g_health_offset << "\n";
        std::cout << "  angles: 0x" << g_view_angles_offset << "\n";
        std::cout << "  team: 0x" << g_team_offset << "\n";
    }

    void update() {
        if (!g_config.enabled) return;

        if (local_player_addr == 0) {
            find_local_player();
            return;
        }

        float yaw, pitch;
        if (!memory::read_memory(local_player_addr + g_view_angles_offset, &yaw, sizeof(yaw)))
            return;
        if (!memory::read_memory(local_player_addr + g_view_angles_offset + 4, &pitch, sizeof(pitch)))
            return;
            
        Vector3 local_angles = {yaw, pitch, 0};

        double px, py, pz;
        if (!memory::read_memory(local_player_addr + g_body_offset, &px, sizeof(px)) ||
            !memory::read_memory(local_player_addr + g_body_offset + 8, &py, sizeof(py)) ||
            !memory::read_memory(local_player_addr + g_body_offset + 16, &pz, sizeof(pz)))
            return;
            
        Vector3 local_pos = {(float)px, (float)py, (float)pz};

        scan_for_players();

        Player* best_target = nullptr;
        float best_fov = g_config.fov;

        for (auto& p : players) {
            if (p.health <= 0) continue;
            
            Vector3 target_pos = get_bone_pos(p);
            float fov = calculate_fov(local_pos, target_pos, local_angles);
            
            if (fov < best_fov) {
                best_fov = fov;
                best_target = &p;
            }
        }

        if (best_target) {
            aim_at(*best_target);
        }

        trigger_check();
    }

    void aim_at(const Player& target) {
        float yaw, pitch;
        memory::read_memory(local_player_addr + g_view_angles_offset, &yaw, sizeof(yaw));
        memory::read_memory(local_player_addr + g_view_angles_offset + 4, &pitch, sizeof(pitch));
        
        Vector3 local_angles = {yaw, pitch, 0};

        double px, py, pz;
        memory::read_memory(local_player_addr + g_body_offset, &px, sizeof(px));
        memory::read_memory(local_player_addr + g_body_offset + 8, &py, sizeof(py));
        memory::read_memory(local_player_addr + g_body_offset + 16, &pz, sizeof(pz));
        
        Vector3 local_pos = {(float)px, (float)py, (float)pz};

        Vector3 target_pos = get_bone_pos(target);
        Vector3 delta = {target_pos.x - local_pos.x, target_pos.y - local_pos.y, target_pos.z - local_pos.z};
        
        float dist = sqrt(delta.x*delta.x + delta.y*delta.y + delta.z*delta.z);
        if (dist < 0.001f) return;

        float target_yaw = atan2(delta.y, delta.x);
        float target_pitch = atan2(delta.z, sqrt(delta.x*delta.x + delta.y*delta.y));

        float smooth_factor = std::max(1.0f, g_config.smooth);
        float new_yaw = local_angles.x + (target_yaw - local_angles.x) / smooth_factor;
        float new_pitch = local_angles.y + (target_pitch - local_angles.y) / smooth_factor;

        if (g_config.silent) {
            memory::write_memory(local_player_addr + g_view_angles_offset, &new_yaw, sizeof(new_yaw));
            memory::write_memory(local_player_addr + g_view_angles_offset + 4, &new_pitch, sizeof(new_pitch));
        } else {
            float dyaw = new_yaw - local_angles.x;
            float dpitch = new_pitch - local_angles.y;
            
            float sensitivity = 100.0f;
            int dx = static_cast<int>(dyaw * sensitivity);
            int dy = static_cast<int>(-dpitch * sensitivity);
            
            move_mouse(dx, dy);
        }
    }

    void trigger_check() {
        if (!g_config.triggerbot) return;
        if (players.empty()) return;
        if (local_player_addr == 0) return;
        
        float yaw, pitch;
        memory::read_memory(local_player_addr + g_view_angles_offset, &yaw, sizeof(yaw));
        memory::read_memory(local_player_addr + g_view_angles_offset + 4, &pitch, sizeof(pitch));
        
        Vector3 local_angles = {yaw, pitch, 0};

        double px, py, pz;
        memory::read_memory(local_player_addr + g_body_offset, &px, sizeof(px));
        memory::read_memory(local_player_addr + g_body_offset + 8, &py, sizeof(py));
        memory::read_memory(local_player_addr + g_body_offset + 16, &pz, sizeof(pz));
        
        Vector3 local_pos = {(float)px, (float)py, (float)pz};

        for (auto& p : players) {
            if (p.health <= 0) continue;
            
            Vector3 target_pos = get_bone_pos(p);
            float fov = calculate_fov(local_pos, target_pos, local_angles);
            if (fov < 2.0f) {
                click_mouse(true);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                click_mouse(false);
                break;
            }
        }
    }
}
