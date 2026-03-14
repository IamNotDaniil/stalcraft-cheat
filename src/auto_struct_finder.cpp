#include <iostream>
#include <vector>
#include <cmath>
#include <map>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <thread>
#include <chrono>
#include "memory.h"

struct PlayerCandidate {
    uintptr_t address;
    double x, y, z;
    float yaw, pitch;
    int health;
    int team;
    bool visible;
    int offsets[10]; // смещения для полей
};

struct StructurePattern {
    int yaw_off;
    int pitch_off;
    int health_off;
    int team_off;
    int visible_off;
    int count;
    double avg_x, avg_y, avg_z;
};

std::vector<memory::MemoryRegion> get_heap_regions() {
    auto regions = memory::get_all_regions();
    std::vector<memory::MemoryRegion> heaps;
    
    std::cout << "Сканирование регионов памяти...\n";
    for (auto& r : regions) {
        size_t size = r.end - r.start;
        if (r.path == "[anon]" && r.perms.find("rw") != std::string::npos && size > 10 * 1024 * 1024) {
            heaps.push_back(r);
            std::cout << "  Найден heap: 0x" << std::hex << r.start << " - 0x" << r.end 
                      << std::dec << " (" << size/1024/1024 << " MB)\n";
        }
    }
    return heaps;
}

void scan_for_entities(const std::vector<memory::MemoryRegion>& heaps) {
    std::vector<PlayerCandidate> candidates;
    std::map<std::string, StructurePattern> patterns;
    
    std::cout << "\nНачинаем сканирование объектов с фильтрацией...\n";
    
    for (const auto& heap : heaps) {
        std::cout << "Сканирование heap 0x" << std::hex << heap.start << std::dec << "\n";
        
        for (uintptr_t addr = heap.start; addr < heap.end - 256; addr += 8) {
            uint8_t buffer[256];
            if (!memory::read_memory(addr, buffer, sizeof(buffer))) continue;
            
            // Ищем координаты (double) в любом месте в первых 64 байтах
            for (int coord_off = 0; coord_off < 64; coord_off += 8) {
                if (coord_off + 24 > 256) break;
                
                double* x = reinterpret_cast<double*>(&buffer[coord_off]);
                double* y = reinterpret_cast<double*>(&buffer[coord_off + 8]);
                double* z = reinterpret_cast<double*>(&buffer[coord_off + 16]);
                
                // Фильтр 1: координаты должны быть разумными
                if (std::isnan(*x) || std::isnan(*y) || std::isnan(*z)) continue;
                if (std::abs(*x) > 5000 || std::abs(*y) > 5000 || std::abs(*z) > 5000) continue;
                if (std::abs(*x) < 10 && std::abs(*y) < 10 && std::abs(*z) < 10) continue; // слишком близко к нулю
                
                // Теперь ищем углы (float) где-то после координат
                for (int angle_off = coord_off + 24; angle_off < coord_off + 64; angle_off += 4) {
                    if (angle_off + 8 > 256) break;
                    
                    float* yaw = reinterpret_cast<float*>(&buffer[angle_off]);
                    float* pitch = reinterpret_cast<float*>(&buffer[angle_off + 4]);
                    
                    // Фильтр 2: углы в разумных пределах
                    if (std::isnan(*yaw) || std::isnan(*pitch)) continue;
                    if (std::abs(*yaw) > 6.3 || std::abs(*pitch) > 3.2) continue; // больше 360°
                    
                    // Ищем здоровье (int) где-то после углов
                    for (int health_off = angle_off + 8; health_off < angle_off + 32; health_off += 4) {
                        if (health_off + 4 > 256) break;
                        
                        int* health = reinterpret_cast<int*>(&buffer[health_off]);
                        
                        // Фильтр 3: здоровье от 0 до 100
                        if (*health < 0 || *health > 100) continue;
                        
                        // Нашли кандидата!
                        PlayerCandidate cand;
                        cand.address = addr;
                        cand.x = *x; cand.y = *y; cand.z = *z;
                        cand.yaw = *yaw; cand.pitch = *pitch;
                        cand.health = *health;
                        cand.offsets[0] = coord_off;      // x offset
                        cand.offsets[1] = angle_off;      // yaw offset
                        cand.offsets[2] = health_off;     // health offset
                        
                        // Ищем команду
                        cand.team = -1;
                        for (int team_off = health_off + 4; team_off < health_off + 20; team_off += 4) {
                            if (team_off + 4 > 256) break;
                            int* team = reinterpret_cast<int*>(&buffer[team_off]);
                            if (*team >= 0 && *team <= 5) {
                                cand.team = *team;
                                cand.offsets[3] = team_off;
                                break;
                            }
                        }
                        
                        // Ищем видимость
                        cand.visible = false;
                        for (int vis_off = health_off + 4; vis_off < health_off + 20; vis_off++) {
                            if (vis_off + 1 > 256) break;
                            uint8_t* vis = &buffer[vis_off];
                            if (*vis == 0 || *vis == 1) {
                                cand.visible = *vis;
                                cand.offsets[4] = vis_off;
                                break;
                            }
                        }
                        
                        candidates.push_back(cand);
                        
                        // Формируем ключ паттерна
                        char pattern_key[256];
                        snprintf(pattern_key, sizeof(pattern_key), "%d_%d_%d_%d_%d",
                                 angle_off - coord_off,           // yaw offset от начала координат
                                 health_off - coord_off,          // health offset от начала координат
                                 (cand.team != -1 ? cand.offsets[3] - coord_off : -1),
                                 (cand.visible ? cand.offsets[4] - coord_off : -1),
                                 health_off - angle_off);         // расстояние от углов до здоровья
                        
                        patterns[pattern_key].count++;
                        patterns[pattern_key].yaw_off = angle_off - coord_off;
                        patterns[pattern_key].health_off = health_off - coord_off;
                        patterns[pattern_key].team_off = (cand.team != -1 ? cand.offsets[3] - coord_off : -1);
                        patterns[pattern_key].visible_off = (cand.visible ? cand.offsets[4] - coord_off : -1);
                        patterns[pattern_key].avg_x += cand.x;
                        patterns[pattern_key].avg_y += cand.y;
                        patterns[pattern_key].avg_z += cand.z;
                        
                        break; // нашли health, выходим
                    }
                    break; // нашли углы, выходим
                }
                // Если нашли координаты и всё проверили, выходим
                if (!candidates.empty() && candidates.back().address == addr) break;
            }
        }
    }
    
    std::cout << "\n=== СТАТИСТИКА ===\n";
    std::cout << "Всего найдено кандидатов: " << candidates.size() << "\n";
    
    if (candidates.empty()) {
        std::cout << "Кандидаты не найдены!\n";
        return;
    }
    
    // Нормализуем средние значения
    for (auto& [key, pat] : patterns) {
        pat.avg_x /= pat.count;
        pat.avg_y /= pat.count;
        pat.avg_z /= pat.count;
    }
    
    // Выводим топ-5 паттернов
    std::vector<std::pair<std::string, StructurePattern>> sorted_patterns;
    for (auto& [key, pat] : patterns) {
        sorted_patterns.push_back({key, pat});
    }
    
    std::sort(sorted_patterns.begin(), sorted_patterns.end(),
              [](auto& a, auto& b) { return a.second.count > b.second.count; });
    
    std::cout << "\n=== ТОП-5 ПАТТЕРНОВ ===\n";
    for (int i = 0; i < std::min(5, (int)sorted_patterns.size()); i++) {
        auto& [key, pat] = sorted_patterns[i];
        std::cout << "Паттерн #" << i+1 << " (найден " << pat.count << " раз):\n";
        std::cout << "  yaw_offset: " << pat.yaw_off << "\n";
        std::cout << "  health_offset: " << pat.health_off << "\n";
        std::cout << "  team_offset: " << pat.team_off << "\n";
        std::cout << "  visible_offset: " << pat.visible_off << "\n";
        std::cout << "  средняя позиция: (" << pat.avg_x << ", " << pat.avg_y << ", " << pat.avg_z << ")\n";
    }
    
    // Берём самый частый паттерн
    auto& best = sorted_patterns[0].second;
    
    std::cout << "\n=== ИТОГОВЫЕ ОФФСЕТЫ ===\n";
    std::cout << "view_angles_offset (от начала координат): " << best.yaw_off << "\n";
    std::cout << "health_offset (от начала координат): " << best.health_off << "\n";
    if (best.team_off != -1)
        std::cout << "team_offset (от начала координат): " << best.team_off << "\n";
    if (best.visible_off != -1)
        std::cout << "visible_offset (от начала координат): " << best.visible_off << "\n";
    
    std::cout << "\nДля использования в aimbot:\n";
    std::cout << "view_angles_offset = 0x" << std::hex << best.yaw_off << "\n";
    std::cout << "health_offset = 0x" << std::hex << best.health_off << "\n";
    if (best.team_off != -1)
        std::cout << "team_offset = 0x" << std::hex << best.team_off << "\n";
    if (best.visible_off != -1)
        std::cout << "visible_offset = 0x" << std::hex << best.visible_off << "\n";
}

int main() {
    std::cout << "=== STALCRAFT X Auto Structure Finder v2 ===\n";
    std::cout << "Ожидание игры...\n";
    
    while (!memory::init("stalcraft")) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    std::cout << "Игра найдена, PID: " << memory::get_pid() << "\n";
    
    auto heaps = get_heap_regions();
    if (heaps.empty()) {
        std::cerr << "Не найдено heap-регионов!\n";
        return 1;
    }
    
    scan_for_entities(heaps);
    
    return 0;
}
