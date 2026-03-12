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

namespace aimbot {
    Config g_config;
    static std::vector<Player> players;
    static int uinput_fd = -1;

    // Оффсеты (теперь заполняются извне)
    static uintptr_t g_entity_list = 0;
    static uintptr_t g_local_player = 0;
    static uintptr_t g_health_offset = 0;
    static uintptr_t g_team_offset = 0;
    static uintptr_t g_visible_offset = 0;
    static uintptr_t g_head_offset = 0;
    static uintptr_t g_body_offset = 0;
    static uintptr_t g_legs_offset = 0;
    static uintptr_t g_view_angles_offset = 0;
    static uintptr_t g_shoot_offset = 0;
    static uintptr_t g_recoil_offset = 0;

    void set_offsets(uintptr_t entity_list, uintptr_t local_player,
                      uintptr_t health_off, uintptr_t team_off, uintptr_t visible_off,
                      uintptr_t head_off, uintptr_t body_off, uintptr_t legs_off,
                      uintptr_t view_angles_off, uintptr_t shoot_off, uintptr_t recoil_off) {
        g_entity_list = entity_list;
        g_local_player = local_player;
        g_health_offset = health_off;
        g_team_offset = team_off;
        g_visible_offset = visible_off;
        g_head_offset = head_off;
        g_body_offset = body_off;
        g_legs_offset = legs_off;
        g_view_angles_offset = view_angles_off;
        g_shoot_offset = shoot_off;
        g_recoil_offset = recoil_off;
    }

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
        strcpy(usetup.name, "Generic Mouse Emulation");
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

    void click_mouse(bool down) {
        if (uinput_fd < 0 && !init_uinput()) return;
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
            default: return p.head;
        }
    }

    float calculate_fov(const Vector3& from, const Vector3& to, const Vector3& angles) {
        Vector3 dir = {to.x - from.x, to.y - from.y, to.z - from.z};
        float len = sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
        if (len < 0.001f) return 0;
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

    void update() {
        if (!g_config.enabled) return;
        if (g_entity_list == 0 || g_local_player == 0) return;

        // Читаем локального игрока
        uintptr_t local_player_addr;
        if (!memory::read_memory(g_local_player, &local_player_addr, sizeof(local_player_addr)))
            return;
        if (local_player_addr == 0) return;

        // Читаем углы локального игрока
        Vector3 local_angles;
        if (!memory::read_memory(local_player_addr + g_view_angles_offset, &local_angles, sizeof(local_angles)))
            return;

        // Читаем позицию локального игрока (например, центр)
        Vector3 local_pos;
        if (!memory::read_memory(local_player_addr + g_body_offset, &local_pos, sizeof(local_pos)))
            return;

        // Читаем список сущностей
        uintptr_t entity_list_ptr;
        if (!memory::read_memory(g_entity_list, &entity_list_ptr, sizeof(entity_list_ptr)))
            return;

        players.clear();
        // Предположим, что это массив указателей на игроков, размер 64
        for (int i = 0; i < 64; ++i) {
            uintptr_t entity_addr;
            if (!memory::read_memory(entity_list_ptr + i * sizeof(uintptr_t), &entity_addr, sizeof(entity_addr)))
                continue;
            if (entity_addr == 0 || entity_addr == local_player_addr) continue;

            Player p;
            p.address = entity_addr;
            memory::read_memory(entity_addr + g_health_offset, &p.health, sizeof(p.health));
            if (p.health <= 0) continue;
            memory::read_memory(entity_addr + g_team_offset, &p.team, sizeof(p.team));
            memory::read_memory(entity_addr + g_visible_offset, &p.visible, sizeof(p.visible));
            if (!p.visible) continue;
            memory::read_memory(entity_addr + g_head_offset, &p.head, sizeof(p.head));
            memory::read_memory(entity_addr + g_body_offset, &p.body, sizeof(p.body));
            memory::read_memory(entity_addr + g_legs_offset, &p.legs, sizeof(p.legs));
            players.push_back(p);
        }

        // Находим ближайшую цель по FOV
        Player* best_target = nullptr;
        float best_fov = g_config.fov;

        for (auto& p : players) {
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

        // No recoil
        if (g_config.no_recoil && g_recoil_offset != 0) {
            // Например, зануляем значение отдачи
            float zero = 0.0f;
            memory::write_memory(local_player_addr + g_recoil_offset, &zero, sizeof(zero));
        }
    }

    void aim_at(const Player& target) {
        if (g_local_player == 0) return;
        uintptr_t local_player_addr;
        if (!memory::read_memory(g_local_player, &local_player_addr, sizeof(local_player_addr)))
            return;

        Vector3 local_pos, local_angles;
        memory::read_memory(local_player_addr + g_body_offset, &local_pos, sizeof(local_pos));
        memory::read_memory(local_player_addr + g_view_angles_offset, &local_angles, sizeof(local_angles));

        Vector3 target_pos = get_bone_pos(target);
        Vector3 delta = {target_pos.x - local_pos.x, target_pos.y - local_pos.y, target_pos.z - local_pos.z};
        float dist = sqrt(delta.x*delta.x + delta.y*delta.y + delta.z*delta.z);
        if (dist < 0.001f) return;

        float yaw = atan2(delta.y, delta.x);
        float pitch = atan2(delta.z, sqrt(delta.x*delta.x + delta.y*delta.y));

        float smooth_factor = std::max(1.0f, g_config.smooth);
        float new_yaw = local_angles.x + (yaw - local_angles.x) / smooth_factor;
        float new_pitch = local_angles.y + (pitch - local_angles.y) / smooth_factor;

        if (g_config.silent) {
            memory::write_memory(local_player_addr + g_view_angles_offset, &new_yaw, sizeof(new_yaw));
            memory::write_memory(local_player_addr + g_view_angles_offset + 4, &new_pitch, sizeof(new_pitch)); // предположим, что pitch через 4 байта
        } else {
            float dyaw = new_yaw - local_angles.x;
            float dpitch = new_pitch - local_angles.y;
            float sensitivity = 10.0f; // подбери под свою чувствительность
            int dx = static_cast<int>(dyaw * sensitivity);
            int dy = static_cast<int>(-dpitch * sensitivity);
            move_mouse(dx, dy);
        }
    }

    void trigger_check() {
        if (!g_config.triggerbot) return;
        if (g_local_player == 0) return;
        uintptr_t local_player_addr;
        if (!memory::read_memory(g_local_player, &local_player_addr, sizeof(local_player_addr)))
            return;
        Vector3 local_angles;
        memory::read_memory(local_player_addr + g_view_angles_offset, &local_angles, sizeof(local_angles));
        Vector3 local_pos;
        memory::read_memory(local_player_addr + g_body_offset, &local_pos, sizeof(local_pos));

        for (auto& p : players) {
            if (!p.visible) continue;
            Vector3 target_pos = get_bone_pos(p);
            float fov = calculate_fov(local_pos, target_pos, local_angles);
            if (fov < 1.0f) {
                click_mouse(true);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                click_mouse(false);
                break;
            }
        }
    }
}
