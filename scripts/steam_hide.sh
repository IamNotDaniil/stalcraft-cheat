#!/bin/bash
# Маскируемся под процесс Steam
export LD_PRELOAD=""
export STEAM_RUNTIME=1
export STEAM_COMPAT_DATA_PATH="/home/iamnotdaniil/.steam/steam/steamapps/compatdata"

# Копируем себя в папку Steam
mkdir -p /home/iamnotdaniil/.steam/steam/ubuntu12_32
cp "$(dirname "$0")/../build/stalcraft_cheat" /home/iamnotdaniil/.steam/steam/ubuntu12_32/steamwebhelper.so

# Запускаем под именем steamwebhelper
cd /home/iamnotdaniil/.steam/steam/ubuntu12_32
exec -a steamwebhelper ./steamwebhelper.so
