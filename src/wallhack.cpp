#include "wallhack.h"
#include "memory.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <thread>
#include <chrono>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <cstring>
#include <unistd.h>

namespace wallhack {
    static std::vector<Player> players;
    static std::vector<Entity> mobs;
    static std::vector<Entity> artifacts;
    static uintptr_t local_player_addr = 0;
    
    static uintptr_t g_health_offset = 0x2c;
    static uintptr_t g_team_offset = 0x3c;
    static uintptr_t g_head_offset = 0x30;
    static uintptr_t g_body_offset = 0x0;
    static uintptr_t g_legs_offset = 0x48;

    static Display* display = nullptr;
    static Window game_window = 0;
    static Window overlay;
    static GC gc;
    static int screen_width = 1920;
    static int screen_height = 1080;
    static int game_x = 0, game_y = 0, game_width = 0, game_height = 0;

    // Поиск окна игры
    Window find_game_window() {
        Window root, parent;
        Window* children;
        unsigned int nchildren;
        
        root = XRootWindow(display, DefaultScreen(display));
        
        // Ищем по имени
        Window ret = XNone;
        if (XQueryTree(display, root, &root, &parent, &children, &nchildren)) {
            for (unsigned int i = 0; i < nchildren; i++) {
                char* name = nullptr;
                if (XFetchName(display, children[i], &name) && name) {
                    if (strstr(name, "STALCRAFT") || strstr(name, "stalcraft") || strstr(name, "Stalcraft")) {
                        std::cout << "Найдено окно игры: " << name << "\n";
                        ret = children[i];
                        XFree(name);
                        break;
                    }
                    XFree(name);
                }
            }
            XFree(children);
        }
        return ret;
    }

    void update_window_position() {
        if (!game_window) return;
        
        XWindowAttributes attr;
        if (XGetWindowAttributes(display, game_window, &attr)) {
            game_x = attr.x;
            game_y = attr.y;
            game_width = attr.width;
            game_height = attr.height;
        }
        
        // Перемещаем оверлей поверх окна игры
        XMoveResizeWindow(display, overlay, game_x, game_y, game_width, game_height);
        XRaiseWindow(display, overlay);
    }

    bool init_overlay() {
        display = XOpenDisplay(nullptr);
        if (!display) {
            std::cerr << "Cannot open display\n";
            return false;
        }

        // Ищем окно игры
        game_window = find_game_window();
        if (!game_window) {
            std::cerr << "Окно игры не найдено\n";
            return false;
        }

        int screen = DefaultScreen(display);
        screen_width = DisplayWidth(display, screen);
        screen_height = DisplayHeight(display, screen);
        
        // Получаем позицию окна игры
        update_window_position();
        
        // Создаем оверлейное окно
        XSetWindowAttributes attr;
        attr.override_redirect = True;
        attr.background_pixel = 0;
        attr.border_pixel = 0;
        attr.colormap = CopyFromParent;
        
        overlay = XCreateWindow(display, RootWindow(display, screen),
                               game_x, game_y, game_width, game_height, 0,
                               CopyFromParent, InputOutput, CopyFromParent,
                               CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWColormap,
                               &attr);
        
        // Делаем окно прозрачным для событий мыши
        XSelectInput(display, overlay, ExposureMask | StructureNotifyMask);
        
        // Устанавливаем "поверх всех окон"
        Atom wm_state = XInternAtom(display, "_NET_WM_STATE", False);
        Atom above = XInternAtom(display, "_NET_WM_STATE_ABOVE", False);
        XChangeProperty(display, overlay, wm_state, XA_ATOM, 32,
                       PropModeReplace, (unsigned char*)&above, 1);
        
        XMapWindow(display, overlay);
        XRaiseWindow(display, overlay);
        
        gc = XCreateGC(display, overlay, 0, nullptr);
        
        XSetForeground(display, gc, 0x00000000);
        XSetBackground(display, gc, 0x00000000);
        
        return true;
    }

    void draw_box(int x, int y, int w, int h, unsigned long color) {
        XSetForeground(display, gc, color);
        XDrawRectangle(display, overlay, gc, x, y, w, h);
    }

    void draw_line(int x1, int y1, int x2, int y2, unsigned long color) {
        XSetForeground(display, gc, color);
        XDrawLine(display, overlay, gc, x1, y1, x2, y2);
    }

    void draw_text(int x, int y, const char* text, unsigned long color) {
        XSetForeground(display, gc, color);
        XDrawString(display, overlay, gc, x, y, text, strlen(text));
    }

    bool world_to_screen(const Vector3& world, int& screen_x, int& screen_y) {
        // ВРЕМЕННАЯ ЗАГЛУШКА - нужно найти view matrix
        // Пока рисуем в центре экрана для теста
        screen_x = game_width / 2;
        screen_y = game_height / 2;
        return true;
    }

    void scan_for_players() {
        players.clear();
        mobs.clear();
        artifacts.clear();
        
        auto regions = memory::get_all_regions();
        for (auto& region : regions) {
            if (region.path.find("[stack") != std::string::npos) continue;
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
                
                int health;
                if (!memory::read_memory(addr + g_health_offset, &health, sizeof(health))) continue;
                if (health < 1 || health > 100) continue;
                
                Player p;
                p.address = addr;
                p.body = {(float)x, (float)y, (float)z};
                p.health = health;
                
                memory::read_memory(addr + g_team_offset, &p.team, sizeof(p.team));
                
                // Разделяем на игроков, мобов и артефакты по здоровью/команде
                if (p.team == 1) players.push_back(p); // игроки
                else if (p.health > 0 && p.health < 20) artifacts.push_back({addr, p.body}); // артефакты
                else mobs.push_back({addr, p.body}); // мобы
                
                if (players.size() + mobs.size() + artifacts.size() >= 64) break;
            }
            if (players.size() + mobs.size() + artifacts.size() >= 64) break;
        }
    }

    void find_local_player() {
        auto regions = memory::get_all_regions();
        
        for (auto& region : regions) {
            if (region.perms.find("r") == std::string::npos) continue;
            
            size_t size = region.end - region.start;
            if (size < 4096 || size > 50 * 1024 * 1024) continue;
            
            for (uintptr_t addr = region.start; addr < region.end - 64; addr += 16) {
                double x, y, z;
                if (!memory::read_memory(addr + 0, &x, sizeof(x))) continue;
                if (!memory::read_memory(addr + 8, &y, sizeof(y))) continue;
                if (!memory::read_memory(addr + 16, &z, sizeof(z))) continue;
                
                if (std::isnan(x) || std::isnan(y) || std::isnan(z)) continue;
                if (std::abs(x) > 10000 || std::abs(y) > 10000 || std::abs(z) > 10000) continue;
                
                int health;
                if (!memory::read_memory(addr + g_health_offset, &health, sizeof(health))) continue;
                if (health < 1 || health > 100) continue;
                
                local_player_addr = addr;
                std::cout << "Найден локальный игрок: 0x" << std::hex << addr << std::dec << "\n";
                return;
            }
        }
    }

    void render() {
        // Очищаем окно
        XSetForeground(display, gc, 0x00000000);
        XFillRectangle(display, overlay, gc, 0, 0, game_width, game_height);
        
        // Рисуем игроков
        for (auto& p : players) {
            int screen_x, screen_y;
            if (world_to_screen(p.body, screen_x, screen_y)) {
                int box_w = 50;
                int box_h = 80;
                
                unsigned long color = 0xFF0000; // красные
                draw_box(screen_x - box_w/2, screen_y - box_h/2, box_w, box_h, color);
                
                char health_text[32];
                snprintf(health_text, sizeof(health_text), "%d HP", p.health);
                draw_text(screen_x - 20, screen_y - box_h/2 - 5, health_text, 0xFFFFFF);
            }
        }
        
        // Рисуем мобов
        for (auto& m : mobs) {
            int screen_x, screen_y;
            if (world_to_screen(m.pos, screen_x, screen_y)) {
                draw_box(screen_x - 20, screen_y - 20, 40, 40, 0xFFA500); // оранжевые
            }
        }
        
        // Рисуем артефакты
        for (auto& a : artifacts) {
            int screen_x, screen_y;
            if (world_to_screen(a.pos, screen_x, screen_y)) {
                draw_box(screen_x - 10, screen_y - 10, 20, 20, 0x00FFFF); // голубые
            }
        }
        
        // Статистика
        char stats[128];
        snprintf(stats, sizeof(stats), "Players: %lu  Mobs: %lu  Artifacts: %lu", 
                 players.size(), mobs.size(), artifacts.size());
        draw_text(10, 20, stats, 0x00FF00);
        
        XFlush(display);
    }

    void run(std::atomic<bool>& running) {
        std::cout << "Инициализация оверлея X11...\n";
        
        if (!init_overlay()) {
            std::cerr << "Не удалось инициализировать оверлей\n";
            return;
        }
        
        std::cout << "Оверлей привязан к окну игры\n";
        
        // Ждем локального игрока
        while (running && local_player_addr == 0) {
            find_local_player();
            if (local_player_addr == 0) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
        
        // Основной цикл
        while (running) {
            update_window_position();
            scan_for_players();
            render();
            
            // Обработка событий окна
            XEvent event;
            while (XPending(display)) {
                XNextEvent(display, &event);
                if (event.type == ConfigureNotify) {
                    update_window_position();
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        
        XDestroyWindow(display, overlay);
        XCloseDisplay(display);
    }

    void set_offsets(uintptr_t health_off, uintptr_t team_off,
                     uintptr_t head_off, uintptr_t body_off, uintptr_t legs_off) {
        if (health_off != 0) g_health_offset = health_off;
        if (team_off != 0) g_team_offset = team_off;
        if (head_off != 0) g_head_offset = head_off;
        if (body_off != 0) g_body_offset = body_off;
        if (legs_off != 0) g_legs_offset = legs_off;
        
        std::cout << "Wallhack offsets set:\n";
        std::cout << "  health: 0x" << std::hex << g_health_offset << "\n";
        std::cout << "  team: 0x" << g_team_offset << "\n";
    }
}
