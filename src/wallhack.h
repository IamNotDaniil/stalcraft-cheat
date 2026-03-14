#pragma once
#include <atomic>
#include <cstdint>

namespace wallhack {
    struct Vector3 {
        float x, y, z;
    };

    struct Player {
        uintptr_t address;
        Vector3 body;
        Vector3 head;
        Vector3 legs;
        int health;
        int team;
    };

    struct Entity {
        uintptr_t address;
        Vector3 pos;
    };

    void run(std::atomic<bool>& running);
    void set_offsets(uintptr_t health_off, uintptr_t team_off,
                     uintptr_t head_off, uintptr_t body_off, uintptr_t legs_off);
}
