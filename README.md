# STALCRAFT X ULTIMATE CHEAT - Termenal-edition

![Version](https://img.shields.io/badge/version-3.0-green)
![Platform](https://img.shields.io/badge/platform-Linux-red)
![Status](https://img.shields.io/badge/status-anticheat_bypassed-success)

## 🚀 Функционал

### 📡 ESP (Радар)
- Отображение всех игроков в терминале
- Цветовая индикация здоровья (🔴 >80, 🟡 60-80, 🟢 <60)
- Компас с направлением на игрока
- 2D радар с видом сверху
- Дистанция до цели

### 🎯 Aimbot
- Автоматическое наведение на цель
- Плавное прицеливание (Smooth)
- Настраиваемый FOV (поле зрения)
- Выбор кости для наведения (голова/тело/ноги)
- Проверка видимости цели
- Фильтр по команде

### 🔫 No Recoil
- Полное подавление отдачи
- Работает с любым оружием
- Не детектится античитом

## 🛠 Установка

```bash
git clone https://github.com/iamnotdaniil/stalcraft-ultimate.git
cd stalcraft-ultimate
mkdir build && cd build
cmake ..
make -j$(nproc)
