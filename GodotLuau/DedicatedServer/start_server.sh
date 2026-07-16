#!/usr/bin/env bash
# ============================================================
#  Servidor dedicado de GodotLuau — Linux
#  Corre tu juego como servidor: ejecuta tus ServerScripts y
#  acepta jugadores, sin jugador local.
# ============================================================

# 1) Cambia esto por la ruta a tu juego EXPORTADO en Linux:
GAME="./MiJuego.x86_64"

# 2) Puerto en el que escucha el servidor:
PORT=25565

# --headless = sin ventana. Quítalo si quieres ver el mundo (cámara libre).
"$GAME" --headless -- --glserver "$PORT"

echo
echo "El servidor se detuvo."
