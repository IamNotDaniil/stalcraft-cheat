#!/bin/bash
export DISPLAY=:0
export WAYLAND_DISPLAY=
export XDG_RUNTIME_DIR=/run/user/$(id -u)
cd "$(dirname "$0")/../build"
exec ./stalcraft_cheat
