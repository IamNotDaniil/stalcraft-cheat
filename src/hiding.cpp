#include "hiding.h"
#include <sys/prctl.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <random>
#include <chrono>

void hide_process(const char* new_name) {
    // 1. Меняем имя процесса (это безопасно)
    prctl(PR_SET_NAME, new_name, 0, 0, 0);
    
    // 2. Пытаемся перезаписать argv[0] (если есть права)
    FILE* f = fopen("/proc/self/cmdline", "w");
    if (f) {
        size_t len = strlen(new_name);
        fwrite(new_name, 1, len, f);
        fputc(0, f);
        fclose(f);
    }
    
    // 3. Добавляем случайную задержку
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100, 500);
    usleep(dis(gen) * 1000);
    
    // 4. НЕ делаем fork(), так как это убивает процесс
    // Вместо этого просто продолжаем работу
}

bool is_antidebug_present() {
    FILE* f = popen("ps aux | grep -E 'exens|easyanticheat|battleye' | grep -v grep", "r");
    if (f) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), f) != NULL) {
            pclose(f);
            return true;
        }
        pclose(f);
    }
    return false;
}
