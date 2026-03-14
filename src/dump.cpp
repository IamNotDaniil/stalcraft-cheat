#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include "memory.h"

int main() {
    std::cout << "=== Stalcraft X Memory Dump ===\n";
    
    if (!memory::init("stalcraft")) {
        std::cerr << "Игра не найдена!\n";
        return 1;
    }
    
    std::cout << "PID: " << memory::get_pid() << "\n";
    
    // Получаем адрес локального игрока
    uintptr_t local_player_addr = 0x781a22c8; // Из предыдущего вывода
    uintptr_t player_ptr;
    
    if (!memory::read_memory(local_player_addr, &player_ptr, sizeof(player_ptr))) {
        std::cerr << "Не удалось прочитать указатель на игрока\n";
        return 1;
    }
    
    std::cout << "Player structure at: 0x" << std::hex << player_ptr << std::dec << "\n";
    
    // Создаём файл для дампа
    std::ofstream dump("player_struct_dump.txt");
    dump << "Player structure at: 0x" << std::hex << player_ptr << std::dec << "\n";
    dump << "Offset (hex) | Offset (dec) | Value (hex) | Value (float) | Value (int)\n";
    dump << "-------------|--------------|-------------|---------------|-----------\n";
    
    // Читаем первые 2048 байт структуры
    std::vector<uint8_t> buffer(2048);
    if (!memory::read_memory(player_ptr, buffer.data(), buffer.size())) {
        std::cerr << "Не удалось прочитать память\n";
        return 1;
    }
    
    // Анализируем каждый участок памяти
    for (size_t offset = 0; offset < buffer.size(); offset += 4) {
        uint32_t int_val = *reinterpret_cast<uint32_t*>(&buffer[offset]);
        float float_val = *reinterpret_cast<float*>(&buffer[offset]);
        
        dump << std::hex << std::setw(8) << std::setfill('0') << offset << " | "
             << std::dec << std::setw(12) << std::setfill(' ') << offset << " | "
             << std::hex << std::setw(8) << std::setfill('0') << int_val << " | "
             << std::fixed << std::setprecision(4) << std::setw(13) << float_val << " | "
             << std::dec << int_val << "\n";
        
        // Выводим в консоль потенциальные углы
        if (offset % 0x40 == 0) { // Каждые 64 байта
            std::cout << "Offset 0x" << std::hex << offset << ": float=" << std::fixed 
                      << std::setprecision(4) << float_val << " int=" << std::dec << int_val << "\n";
        }
    }
    
    dump.close();
    std::cout << "\nДамп сохранён в player_struct_dump.txt\n";
    std::cout << "Ищи в файле значения где float в диапазоне -3.14 до 3.14 (yaw) и -1.57 до 1.57 (pitch)\n";
    
    return 0;
}
