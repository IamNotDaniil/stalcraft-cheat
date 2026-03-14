#include "memory.h"
#include <iostream>
#include <vector>

int main() {
    std::cout << "=== Тест регионов памяти ===\n";
    
    if (!memory::init("stalcraft")) {
        std::cerr << "Игра не найдена!\n";
        return 1;
    }
    
    std::cout << "PID: " << memory::get_pid() << "\n";
    
    auto regions = memory::get_all_regions();
    std::cout << "Всего регионов: " << regions.size() << "\n\n";
    
    int count = 0;
    for (auto& r : regions) {
        size_t size = r.end - r.start;
        std::cout << "Регион " << ++count << ":\n";
        std::cout << "  Адрес: 0x" << std::hex << r.start << " - 0x" << r.end << std::dec << "\n";
        std::cout << "  Размер: " << size/1024 << " KB\n";
        std::cout << "  Права: " << r.perms << "\n";
        std::cout << "  Путь: " << r.path << "\n\n";
        
        if (count >= 20) break;
    }
    
    return 0;
}
