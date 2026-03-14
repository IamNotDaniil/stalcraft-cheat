#include "memory.h"
#include <iostream>
#include <vector>
#include <iomanip>

int main() {
    std::cout << "=== Поиск анонимных регионов ===\n";
    
    if (!memory::init("stalcraft")) {
        std::cerr << "Игра не найдена!\n";
        return 1;
    }
    
    std::cout << "PID: " << memory::get_pid() << "\n\n";
    
    auto regions = memory::get_all_regions();
    std::cout << "Всего регионов: " << regions.size() << "\n\n";
    
    int anon_count = 0;
    for (auto& r : regions) {
        size_t size = r.end - r.start;
        
        // Показываем только большие анонимные регионы
        if (r.path == "[anon]" && size > 1024*1024) {
            anon_count++;
            std::cout << "Анонимный регион " << anon_count << ":\n";
            std::cout << "  Адрес: 0x" << std::hex << r.start << " - 0x" << r.end << std::dec << "\n";
            std::cout << "  Размер: " << size/1024/1024 << " MB\n";
            std::cout << "  Права: " << r.perms << "\n\n";
        }
    }
    
    std::cout << "Всего больших анонимных регионов: " << anon_count << "\n";
    
    return 0;
}
