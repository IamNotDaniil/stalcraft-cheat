#!/bin/bash
# Путь к исполняемому файлу чита
CHEAT_BIN="$(dirname "$0")/../build/stalcraft_cheat"

# Проверка прав на /dev/uinput (нужны root или группа input)
if [ ! -w /dev/uinput ]; then
    echo "Нет прав на запись /dev/uinput. Попробуй запустить с sudo или добавь пользователя в группу input."
    exit 1
fi

# Запускаем чит
exec "$CHEAT_BIN"
