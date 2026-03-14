#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <signal.h>
#include <unistd.h>
#include "memory.h"
#include "wallhack.h"
#include "offset_finder.h"
#include "hiding.h"

std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running = false;
}

int main() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    hide_process("Xorg");
    
    std::cout << "=== STALCRAFT X Wallhack ===\n";
    std::cout << "Ожидание запуска игры...\n";

    while (!memory::init("stalcraft")) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    std::cout << "Игра найдена, PID: " << memory::get_pid() << "\n";

    offset_finder::find_offsets();

    wallhack::set_offsets(
        offset_finder::g_offsets.health_offset,
        offset_finder::g_offsets.team_offset,
        offset_finder::g_offsets.head_offset,
        offset_finder::g_offsets.body_offset,
        offset_finder::g_offsets.legs_offset
    );

    std::cout << "Запуск оверлея...\n";
    std::thread overlay_thread(wallhack::run, std::ref(g_running));

    std::cout << "Wallhack запущен! Нажми Ctrl+C для выхода.\n";
    
    overlay_thread.join();
    memory::cleanup();
    return 0;
}
