-- > GodotLuau — PimpoliDev
-- ══════════════════════════════════════════════════════════════════
--  SISTEMA DE MOVIMIENTO 2D
--  Archivo: StarterPlayerScripts/PlayerController2D
--
--  Este script controla el comportamiento del jugador 2D.
--  Cambia las variables de configuración para personalizar el juego.
-- ══════════════════════════════════════════════════════════════════

-- ┌──────────────────────────────────────────────────────────────┐
-- │  TIPO DE MOVIMIENTO                                          │
-- │                                                              │
-- │  0 = Platformer → Con física y gravedad (tipo Mario)        │
-- │                   El personaje puede caer y saltar          │
-- │                   Controles: A/D o ←/→ para moverse        │
-- │                              Space, W o ↑ para saltar      │
-- │                                                              │
-- │  1 = TopDown    → Sin gravedad (tipo Zelda/RPG)             │
-- │                   El personaje se mueve en 8 direcciones    │
-- │                   Controles: WASD o flechas                 │
-- └──────────────────────────────────────────────────────────────┘
local MOVEMENT_TYPE = 0  -- ← Cambia aquí: 0=Platformer, 1=TopDown

-- ┌──────────────────────────────────────────────────────────────┐
-- │  CONFIGURACIÓN DE CÁMARA                                     │
-- │  (Solo aplica en modo TopDown — Platformer tiene cámara fija)│
-- │                                                              │
-- │  1 = Fija       → La cámara sigue al jugador al instante   │
-- │  2 = Suave      → La cámara sigue con un pequeño retraso   │
-- │  3 = Combinada  → Retraso al moverse, centra al detenerse  │
-- └──────────────────────────────────────────────────────────────┘
local CAMERA_MODE = 1  -- ← Solo aplica en TopDown

-- ┌──────────────────────────────────────────────────────────────┐
-- │  VALORES DE MOVIMIENTO                                       │
-- └──────────────────────────────────────────────────────────────┘
local WALK_SPEED = 200    -- Velocidad normal (píxeles/segundo)
local RUN_SPEED  = 320    -- Velocidad corriendo (mantén LeftShift)

-- ══════════════════════════════════════════════════════════════════
--  INICIO DEL SCRIPT
-- ══════════════════════════════════════════════════════════════════

local Players = game:GetService("Players")
local player  = Players.LocalPlayer

if not player then
    warn("[PlayerController2D] No se encontró LocalPlayer. Abortando.")
    return
end

-- Aplicar configuración al jugador 2D
-- El motor C++ lee estas propiedades para saber cómo comportarse
player.MovementType = MOVEMENT_TYPE
player.CameraMode   = CAMERA_MODE

-- Buscar Humanoid 2D para información de salud
local humanoid2d = player:FindFirstChild("Humanoid")

-- Log de configuración
local tipo_str = (MOVEMENT_TYPE == 0) and "Platformer (con física)" or "TopDown (sin gravedad)"
print("[GodotLuau] PlayerController2D activo.")
print("[GodotLuau]   Tipo de movimiento: " .. tipo_str)
print("[GodotLuau]   Modo de cámara:     " .. CAMERA_MODE)

-- ══════════════════════════════════════════════════════════════════
--  BUCLE PRINCIPAL DEL JUGADOR
-- ══════════════════════════════════════════════════════════════════
while task.wait(0) do
    if not player then break end

    -- Correr con LeftShift (ajustar WalkSpeed del jugador 2D)
    local is_running = UserInputService:IsKeyDown("LeftShift")

    -- En el Player2D la velocidad se controla directamente
    -- Las propiedades WalkSpeed se leen del C++ en _physics_process
    -- Aquí podemos modificar el personaje de otras formas

    -- ── ZONA DE CÓDIGO PERSONALIZADO ──────────────────────────────
    --
    -- Ejemplo Platformer: doble salto
    -- (implementación avanzada que puedes agregar)
    --
    -- Ejemplo TopDown: cambiar a 4 direcciones con X
    -- if UserInputService:IsKeyDown("X") then
    --     -- lógica de cambio de modo
    -- end
    --
    -- Ejemplo: Interactuar con objetos con E
    -- if UserInputService:IsKeyDown("E") then
    --     -- lógica de interacción
    -- end
    -- ─────────────────────────────────────────────────────────────
end

print("[GodotLuau] PlayerController2D finalizado.")
