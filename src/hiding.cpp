#include "hiding.h"
#include <sys/prctl.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <fstream>

void hide_process(const char* new_name) {
    // Изменяем имя процесса через prctl
    prctl(PR_SET_NAME, new_name, 0, 0, 0);

    // Перезаписываем argv[0] (для скрытия в ps aux)
    FILE* f = fopen("/proc/self/cmdline", "r+");
    if (f) {
        size_t len = strlen(new_name);
        fwrite(new_name, 1, len, f);
        fputc(0, f); // null terminator
        fclose(f);
    }

    // Дополнительно: удаляемся из списка процессов? Можно использовать unshare, но это сложнее.
    // Просто оставим так.
}
