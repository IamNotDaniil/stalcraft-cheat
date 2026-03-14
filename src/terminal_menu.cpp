#include "terminal_menu.h"
#include "aimbot.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <ncurses.h>
#include <cstring>

namespace terminal_menu {
    static std::atomic<bool> menu_running{true};
    static std::atomic<bool> redraw{true};
    
    void draw_menu() {
        clear();
        mvprintw(1, 2, "=== STALCRAFT X CHEAT ===");
        mvprintw(2, 2, "==========================");
        
        mvprintw(4, 2, "F1 - Toggle Aimbot:      [%s]", aimbot::g_config.enabled ? "ON " : "OFF");
        mvprintw(5, 2, "F2 - Toggle Triggerbot:  [%s]", aimbot::g_config.triggerbot ? "ON " : "OFF");
        mvprintw(6, 2, "F3 - Toggle Silent Aim:  [%s]", aimbot::g_config.silent ? "ON " : "OFF");
        mvprintw(7, 2, "F4 - Toggle No Recoil:   [%s]", aimbot::g_config.no_recoil ? "ON " : "OFF");
        
        mvprintw(9, 2, "FOV: %.1f", aimbot::g_config.fov);
        mvprintw(10, 2, "Smooth: %.1f", aimbot::g_config.smooth);
        
        const char* bones[] = {"Head", "Body", "Legs"};
        mvprintw(11, 2, "Bone: %s", bones[aimbot::g_config.bone]);
        
        mvprintw(13, 2, "Arrow Up/Down - Change FOV");
        mvprintw(14, 2, "Arrow Left/Right - Change Smooth");
        mvprintw(15, 2, "Page Up/Down - Change Bone");
        mvprintw(16, 2, "Q - Quit");
        
        mvprintw(18, 2, "Status: %s", aimbot::g_config.enabled ? "Aimbot ACTIVE" : "Aimbot DISABLED");
        
        refresh();
    }
    
    void handle_input() {
        int ch = getch();
        switch(ch) {
            case KEY_F(1):
                aimbot::g_config.enabled = !aimbot::g_config.enabled;
                redraw = true;
                break;
            case KEY_F(2):
                aimbot::g_config.triggerbot = !aimbot::g_config.triggerbot;
                redraw = true;
                break;
            case KEY_F(3):
                aimbot::g_config.silent = !aimbot::g_config.silent;
                redraw = true;
                break;
            case KEY_F(4):
                aimbot::g_config.no_recoil = !aimbot::g_config.no_recoil;
                redraw = true;
                break;
            case KEY_UP:
                aimbot::g_config.fov += 1.0f;
                if (aimbot::g_config.fov > 180.0f) aimbot::g_config.fov = 180.0f;
                redraw = true;
                break;
            case KEY_DOWN:
                aimbot::g_config.fov -= 1.0f;
                if (aimbot::g_config.fov < 0.0f) aimbot::g_config.fov = 0.0f;
                redraw = true;
                break;
            case KEY_RIGHT:
                aimbot::g_config.smooth += 1.0f;
                if (aimbot::g_config.smooth > 20.0f) aimbot::g_config.smooth = 20.0f;
                redraw = true;
                break;
            case KEY_LEFT:
                aimbot::g_config.smooth -= 1.0f;
                if (aimbot::g_config.smooth < 1.0f) aimbot::g_config.smooth = 1.0f;
                redraw = true;
                break;
            case KEY_NPAGE: // Page Up
                aimbot::g_config.bone = (aimbot::g_config.bone + 1) % 3;
                redraw = true;
                break;
            case KEY_PPAGE: // Page Down
                aimbot::g_config.bone = (aimbot::g_config.bone - 1 + 3) % 3;
                redraw = true;
                break;
            case 'q':
            case 'Q':
                menu_running = false;
                break;
        }
    }
    
    void run(std::atomic<bool>& running) {
        // Инициализация ncurses
        initscr();
        cbreak();
        noecho();
        keypad(stdscr, TRUE);
        nodelay(stdscr, TRUE);
        curs_set(0);
        
        while (menu_running && running) {
            handle_input();
            if (redraw) {
                draw_menu();
                redraw = false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        // Завершение ncurses
        endwin();
    }
}
