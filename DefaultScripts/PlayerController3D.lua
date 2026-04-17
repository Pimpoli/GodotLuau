-- > GodotLuau — PimpoliDev
-- ══════════════════════════════════════════════════════════════════
--  SISTEMA DE MOVIMIENTO Y CÁMARA 3D
--  Archivo: StarterPlayerScripts/PlayerController3D
--
--  Este script se ejecuta automáticamente al iniciar el juego.
--  Puedes modificarlo libremente para personalizar el comportamiento
--  del jugador. Los comentarios explican cada sección.
-- ══════════════════════════════════════════════════════════════════

-- ┌──────────────────────────────────────────────────────────────┐
-- │  CONFIGURACIÓN DE CÁMARA                                     │
-- │                                                              │
-- │  1 = Fija       → Sigue al jugador al instante              │
-- │  2 = Suave      → Sigue con un pequeño retraso elegante     │
-- │  3 = Combinada  → Retraso al moverse, centra al detenerse   │
-- └──────────────────────────────────────────────────────────────┘
local CAMERA_MODE = 1  -- ← Cambia este número (1, 2 o 3)

-- ┌──────────────────────────────────────────────────────────────┐
-- │  VALORES DE MOVIMIENTO                                       │
-- │  Modifica estos números para personalizar el movimiento      │
-- └──────────────────────────────────────────────────────────────┘
local WALK_SPEED  = 16    -- Velocidad normal caminando (studs/segundo)
local RUN_SPEED   = 24    -- Velocidad corriendo (mantén LeftShift)
local JUMP_POWER  = 20    -- Fuerza del salto (más alto = salta más)

-- ══════════════════════════════════════════════════════════════════
--  INICIO DEL SCRIPT - Obtener referencias del motor
-- ══════════════════════════════════════════════════════════════════

-- Obtener el servicio de jugadores
local Players = game:GetService("Players")
local player  = Players.LocalPlayer

-- Verificar que el jugador existe
if not player then
    warn("[PlayerController3D] No se encontró LocalPlayer. Abortando.")
    return
end

-- Buscar el Humanoid (controla velocidad y salud)
local humanoid = player:FindFirstChild("Humanoid")

-- Aplicar configuración inicial
if humanoid then
    humanoid.WalkSpeed = WALK_SPEED
    humanoid.JumpPower = JUMP_POWER
    print("[GodotLuau] PlayerController3D activo.")
    print("[GodotLuau]   WalkSpeed = " .. WALK_SPEED)
    print("[GodotLuau]   JumpPower = " .. JUMP_POWER)
end

-- Aplicar el modo de cámara elegido
-- El motor C++ lee esta propiedad y aplica el comportamiento correcto
player.CameraMode = CAMERA_MODE
print("[GodotLuau] Modo de cámara: " .. CAMERA_MODE ..
    (CAMERA_MODE == 1 and " (Fija)" or
     CAMERA_MODE == 2 and " (Suave)" or
     " (Combinada)"))

-- ══════════════════════════════════════════════════════════════════
--  BUCLE PRINCIPAL DEL JUGADOR
--  Se ejecuta cada frame. Aquí puedes agregar lógica custom.
-- ══════════════════════════════════════════════════════════════════
while task.wait(0) do
    -- Verificar que el jugador y humanoid siguen existiendo
    if not player or not humanoid then
        break
    end

    -- Correr al mantener LeftShift
    local is_running = UserInputService:IsKeyDown("LeftShift")
    local target_speed = is_running and RUN_SPEED or WALK_SPEED

    -- Solo actualizar si cambió la velocidad (optimización)
    if humanoid.WalkSpeed ~= target_speed then
        humanoid.WalkSpeed = target_speed
    end

    -- ── ZONA DE CÓDIGO PERSONALIZADO ──────────────────────────────
    -- Aquí puedes agregar tu propia lógica:
    --
    -- Ejemplo: Recuperar vida con E
    -- if UserInputService:IsKeyDown("E") then
    --     humanoid.Health = humanoid.Health + 1
    -- end
    --
    -- Ejemplo: Teletransportarse con G
    -- if UserInputService:IsKeyDown("G") then
    --     player.Position = Vector3.new(0, 5, 0)
    -- end
    -- ─────────────────────────────────────────────────────────────
end

print("[GodotLuau] PlayerController3D finalizado.")
