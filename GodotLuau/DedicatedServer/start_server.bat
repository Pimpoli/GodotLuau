@echo off
REM ============================================================
REM  Servidor dedicado de GodotLuau (estilo minecraft_server.jar)
REM  Corre tu juego como servidor: ejecuta tus ServerScripts y
REM  acepta jugadores, sin jugador local.
REM ============================================================

REM 1) Cambia esto por el nombre de tu juego EXPORTADO (.exe):
set GAME=MiJuego.exe

REM 2) Puerto en el que escucha el servidor:
set PORT=25565

REM --headless = sin ventana. Quitalo si quieres ver el mundo (camara libre).
"%GAME%" --headless -- --glserver %PORT%

echo.
echo El servidor se detuvo.
pause
