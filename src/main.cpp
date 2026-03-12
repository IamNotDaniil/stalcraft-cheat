#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <signal.h>
#include <unistd.h>
#include <sys/prctl.h>
#include "memory.h"
#include "pattern_scanner.h"
#include "aimbot.h"
#include "overlay.h"
#include "hiding.h"

std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running = false;
}

int main() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Скрываем процесс
    hide_process("systemd-resolved");

    // Инициализация памяти
    if (!memory::init("stalcraftw.exe")) {
        std::cerr << "Не удалось найти процесс stalcraftw.exe" << std::endl;
        return 1;
    }

    // Получаем базовый адрес и размер модуля
    uintptr_t base = memory::get_module_base("stalcraftw.exe");
    size_t size = memory::get_module_size("stalcraftw.exe");
    if (base == 0 || size == 0) {
        std::cerr << "Не удалось получить информацию о модуле" << std::endl;
        return 1;
    }

    // Автоматический поиск оффсетов
    if (!pattern_scanner::find_offsets(base, size)) {
        std::cerr << "Не удалось найти все сигнатуры. Проверь их актуальность." << std::endl;
        // Можно продолжить, если некоторые не критичны
    }

    // Передаём найденные адреса в аимбот
    aimbot::set_offsets(
        pattern_scanner::entity_list_addr,
        pattern_scanner::local_player_addr,
        pattern_scanner::health_offset,
        pattern_scanner::team_offset,
        pattern_scanner::visible_offset,
        pattern_scanner::head_offset,
        pattern_scanner::body_offset,
        pattern_scanner::legs_offset,
        pattern_scanner::view_angles_offset,
        pattern_scanner::shoot_offset,
        pattern_scanner::recoil_offset
    );

    // Запуск оверлея в отдельном потоке
    std::thread overlay_thread(overlay::run, std::ref(g_running));

    // Основной цикл аима
    while (g_running) {
        aimbot::update();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    overlay_thread.join();
    memory::cleanup();
    return 0;
}
