#pragma once
#include <cstdint>
#include <vector>
#include <atomic>

namespace aimbot {
    struct Vector3 {
        float x, y, z;
    };

    struct Player {
        uintptr_t address;
        Vector3 head;
        Vector3 body;
        Vector3 legs;
        int health;
        int team;
        bool visible;
    };

    struct Config {
        bool enabled = true;
        float fov = 10.0f;
        bool draw_fov = true;
        float fov_color[3] = {1.0f, 0.0f, 0.0f};
        float smooth = 5.0f;
        int bone = 0; // 0=head,1=body,2=legs
        bool triggerbot = false;
        bool silent = false;
        bool no_recoil = true;
        int hotkey = 0; // будет обрабатываться в оверлее
    };

    extern Config g_config;
    void update();
    void aim_at(const Player& target);
    void trigger_check();
    void set_offsets(uintptr_t entity_list, uintptr_t local_player,
                      uintptr_t health_off, uintptr_t team_off, uintptr_t visible_off,
                      uintptr_t head_off, uintptr_t body_off, uintptr_t legs_off,
                      uintptr_t view_angles_off, uintptr_t shoot_off, uintptr_t recoil_off);
}
