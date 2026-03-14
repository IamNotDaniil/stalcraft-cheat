#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <signal.h>
#include <unistd.h>
#include "memory.h"
#include "offset_finder.h"
#include "aimbot.h"
#include "terminal_menu.h"
#include "hiding.h"

std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running = false;
}

int main() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    hide_process("steamwebhelper");
    
    std::cout << "=== Stalcraft X Cheat ===\n";
    std::cout << "Ожидание запуска игры...\n";

    while (!memory::init("stalcraft")) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    std::cout << "Процесс найден, PID: " << memory::get_pid() << std::endl;

    // Динамический поиск оффсетов
    if (!offset_finder::find_offsets()) {
        std::cout << "Не удалось найти оффсеты автоматически.\n";
    }

    // Передаём найденные оффсеты в аимбот
    aimbot::set_offsets(
        offset_finder::g_offsets.entity_list_addr,
        offset_finder::g_offsets.local_player_addr,
        offset_finder::g_offsets.health_offset,
        offset_finder::g_offsets.team_offset,
        offset_finder::g_offsets.visible_offset,
        offset_finder::g_offsets.head_offset,
        offset_finder::g_offsets.body_offset,
        offset_finder::g_offsets.legs_offset,
        offset_finder::g_offsets.view_angles_offset,
        offset_finder::g_offsets.shoot_offset,
        offset_finder::g_offsets.recoil_offset
    );

    std::cout << "Запуск меню...\n";
    std::thread menu_thread(terminal_menu::run, std::ref(g_running));

    std::cout << "Чит запущен! Нажми Ctrl+C для выхода.\n";
    
    int last_print = 0;
    while (g_running) {
        aimbot::update();
        
        // Показываем статус каждые 5 секунд
        last_print++;
        if (last_print > 5000) {
            std::cout << "Аимбот активен. FOV: " << aimbot::g_config.fov 
                      << ", Smooth: " << aimbot::g_config.smooth 
                      << ", Bone: " << aimbot::g_config.bone << "\n";
            last_print = 0;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    menu_thread.join();
    memory::cleanup();
    return 0;
}
