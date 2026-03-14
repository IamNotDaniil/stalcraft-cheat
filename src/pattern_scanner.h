#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace pattern_scanner {
    struct Signature {
        std::string name;
        std::string pattern;
        int32_t offset;
        int32_t deref_count;
        bool relative;
        uintptr_t result;
    };

    struct ModuleInfo {
        std::string name;
        uintptr_t base;
        size_t size;
        bool is_executable;
    };

    std::optional<uintptr_t> scan_pattern(uintptr_t start, size_t size, const std::string& pattern, int32_t offset = 0, bool relative = false, int deref = 0);
    bool get_process_modules();
    bool find_offsets();

    extern std::vector<ModuleInfo> g_modules;
    extern uintptr_t entity_list_addr;
    extern uintptr_t local_player_addr;
    extern uintptr_t health_offset;
    extern uintptr_t team_offset;
    extern uintptr_t visible_offset;
    extern uintptr_t head_offset;
    extern uintptr_t body_offset;
    extern uintptr_t legs_offset;
    extern uintptr_t view_angles_offset;
    extern uintptr_t shoot_offset;
    extern uintptr_t recoil_offset;
}
