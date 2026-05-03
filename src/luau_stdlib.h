#ifndef LUAU_STDLIB_H
#define LUAU_STDLIB_H

// ════════════════════════════════════════════════════════════════════
//  GodotLuau stdlib — services implemented in pure Lua.
//  Runs at the start of each ScriptNodeBase before the user's script.
//  Services are stored in _G["__SVC_ServiceName"] so that
//  game:GetService() can find them without a C++ node.
////
//  GodotLuau stdlib — servicios implementados en Lua puro.
//  Se ejecuta al inicio de cada ScriptNodeBase antes del script del usuario.
//  Los servicios se guardan en _G["__SVC_NombreServicio"] para que
//  game:GetService() los encuentre sin necesidad de un nodo C++.
// ════════════════════════════════════════════════════════════════════

static const char* LUAU_STDLIB_CODE = R"LUAU(

-- ══════════════════════════════════════════════════════════════════════
--  class(base) — Roblox-style OOP inheritance with setmetatable
-- ////
--  class(base) — herencia OOP estilo Roblox con setmetatable
-- ══════════════════════════════════════════════════════════════════════
function class(base)
    local cls = {}
    cls.__index = cls
    if base then
        setmetatable(cls, { __index = base })
    end
    cls.new = function(...)
        local self = setmetatable({}, cls)
        if self.init then self:init(...) end
        return self
    end
    cls.super = base
    return cls
end

-- ══════════════════════════════════════════════════════════════════════
--  Enum — exact Roblox enumeration constants
-- ////
--  Enum — constantes de enumeración exactas de Roblox
-- ══════════════════════════════════════════════════════════════════════
Enum = {
    EasingStyle = {
        Linear = 0, Sine = 1, Quad = 2, Cubic = 3,
        Quart = 4, Quint = 5, Bounce = 6, Elastic = 7,
        Back = 8, Exponential = 9,
    },
    EasingDirection = { In = 0, Out = 1, InOut = 2 },
    HumanoidStateType = {
        None = 0, Running = 1, Jumping = 2, Landed = 3,
        Swimming = 4, Climbing = 5, Seated = 6, Dead = 7,
        Freefall = 8, Flying = 9, GettingUp = 10, FallingDown = 11,
        Ragdoll = 12, StrafingNoWalk = 13, Frozen = 14,
    },
    UserInputType = {
        None = 0, MouseButton1 = 1, MouseButton2 = 2, MouseButton3 = 3,
        MouseWheel = 4, MouseMovement = 5, Touch = 7, Gamepad1 = 8,
        Gamepad2 = 9, Keyboard = 10, Focus = 11, Accelerometer = 12,
        Gyro = 13, TextInput = 14,
    },
    KeyCode = {
        Unknown = 0, Backspace = 8, Tab = 9, Return = 13, Escape = 27,
        Space = 32, Quote = 39, Comma = 44, Minus = 45, Period = 46,
        Slash = 47, Zero = 48, One = 49, Two = 50, Three = 51, Four = 52,
        Five = 53, Six = 54, Seven = 55, Eight = 56, Nine = 57,
        Semicolon = 59, Equals = 61, LeftBracket = 91, Backslash = 92,
        RightBracket = 93, Backquote = 96,
        A = 65, B = 66, C = 67, D = 68, E = 69, F = 70, G = 71, H = 72,
        I = 73, J = 74, K = 75, L = 76, M = 77, N = 78, O = 79, P = 80,
        Q = 81, R = 82, S = 83, T = 84, U = 85, V = 86, W = 87, X = 88,
        Y = 89, Z = 90,
        LeftShift = 304, RightShift = 303, LeftControl = 306, RightControl = 305,
        LeftAlt = 308, RightAlt = 307, CapsLock = 301, NumLock = 300,
        ScrollLock = 302, LeftMeta = 310, RightMeta = 309,
        Insert = 277, Home = 278, PageUp = 280, Delete = 127,
        End = 279, PageDown = 281,
        Up = 273, Down = 274, Left = 276, Right = 275,
        F1 = 282, F2 = 283, F3 = 284, F4 = 285, F5 = 286,
        F6 = 287, F7 = 288, F8 = 289, F9 = 290, F10 = 291,
        F11 = 292, F12 = 293,
        KeypadZero = 256, KeypadOne = 257, KeypadTwo = 258, KeypadThree = 259,
        KeypadFour = 260, KeypadFive = 261, KeypadSix = 262, KeypadSeven = 263,
        KeypadEight = 264, KeypadNine = 265, KeypadPeriod = 266,
        KeypadDivide = 267, KeypadMultiply = 268, KeypadMinus = 269,
        KeypadPlus = 270, KeypadEnter = 271,
    },
    Material = {
        Plastic = 256, SmoothPlastic = 272, Metal = 1280, Neon = 288,
        Wood = 512, WoodPlanks = 528, Marble = 784, Slate = 800,
        Concrete = 816, Granite = 832, Foil = 1296, Grass = 1024,
        SandStone = 1040, Sand = 1280, Fabric = 1312, Ice = 1536,
        Glacier = 1552, Snow = 1568, Cobblestone = 1584,
        Brick = 784, Ground = 1040, Mud = 1056, Rock = 816,
        Basalt = 848, CrackedLava = 864, Asphalt = 1072, LeafyGrass = 1288,
        Salt = 1552, Limestone = 1584, Pavement = 1600,
        Air = 1, Water = 2, Forcefield = 1536, Fire = 1792,
        DiamondPlate = 1072, Corrodedmetal = 1088,
    },
    NormalId = { Top = 1, Bottom = 2, Front = 3, Back = 4, Right = 5, Left = 6 },
    CameraMode = { Classic = 0, LockFirstPerson = 1 },
    PartType = { Block = 0, Sphere = 1, Cylinder = 2 },
    SurfaceType = { Smooth = 0, Weld = 1, Studs = 2, Inbox = 3, Hinge = 4, Motor = 5, SteppingMotor = 6 },
    MeshType = { Head = 0, Torso = 1, Wedge = 2, Prism = 3, Pyramid = 4, ParallelRamp = 5, RightAngleRamp = 6, CornerWedge = 7, Sphere = 8, Cylinder = 9, FileMesh = 10 },
    Font = { Legacy = 0, Arial = 1, ArialBold = 2, SourceSans = 3, SourceSansBold = 4, SourceSansLight = 5, SourceSansItalic = 6, Bodoni = 7, Garamond = 8, Cartoon = 9, Code = 10, Highway = 11, SciFi = 12, Arcade = 13, Fantasy = 14, Antique = 15, Gotham = 16, GothamMedium = 17, GothamBold = 18, GothamBlack = 19, Roboto = 20, RobotoCondensed = 21, RobotoMono = 22 },
    TextXAlignment = { Left = 0, Center = 1, Right = 2 },
    TextYAlignment = { Top = 0, Center = 1, Bottom = 2 },
    ScaleType = { Stretch = 0, Slice = 1, Tile = 2, Fit = 3, Crop = 4 },
    ZIndexBehavior = { Global = 0, Sibling = 1 },
    SoundPriority = { Ambient = 0, Music = 1, Sfx = 2, Voice = 3 },
    RollOffMode = { Linear = 0, Inverse = 1, LinearSquare = 2, InverseTapered = 3 },
    TeamColor = { BrightRed = 0, BrightBlue = 1, BrightYellow = 2, BrightGreen = 3, White = 4, Black = 5 },
    ForceLimitMode = { Magnitude = 0, PerAxis = 1 },
    FormFactor = { Symmetric = 0, Brick = 1, Plate = 2, Custom = 3 },
    MessageType = { MessageOutput = 0, MessageInfo = 1, MessageWarning = 2, MessageError = 3 },
    ThumbnailType = { HeadShot = 0, AvatarBust = 1, AvatarThumbnail = 2 },
    InfoType = { Asset = 0, Product = 1, GamePass = 2, Subscription = 3, Bundle = 4 },
    ProductPurchaseDecision = { NotProcessedYet = 0, PurchaseGranted = 1 },
    TeleportType = { ToPlace = 0, ToInstance = 1, ToReservedServer = 2 },
    HumanoidRigType = { R6 = 0, R15 = 1 },
    AnimationPriority = { Idle = 0, Movement = 1, Action = 2, Core = 3, Action2 = 4, Action3 = 4, Action4 = 4 },
    RaycastFilterType = { Include = 0, Exclude = 1 },
    MouseButton = { Left = 1, Right = 2, Middle = 3 },
    UserCFrame = { Head = 0, LeftHand = 1, RightHand = 2 },
    ContextActionResult = { Pass = 0, Sink = 1 },
    Platform = { Windows = 0, OSX = 1, IOS = 2, Android = 3, Xbox = 4 },
    DeviceType = { Desktop = 0, Phone = 1, Tablet = 2, Console = 3 },
    AccessoryType = { Hat = 0, Hair = 1, Face = 2, Neck = 3, Shoulder = 4, Front = 5, Back = 6, Waist = 7 },
    RunContext = { Legacy = 0, Server = 1, Client = 2 },
    RenderFidelity = { Automatic = 0, Precise = 1, Performance = 2 },
    CollisionFidelity = { Default = 0, Box = 1, Hull = 2, Disabled = 3, PreciseConvexDecomposition = 4 },
}

-- ══════════════════════════════════════════════════════════════════════
--  RaycastParams — parameters for workspace:Raycast()
-- ////
--  RaycastParams — parámetros para workspace:Raycast()
-- ══════════════════════════════════════════════════════════════════════
RaycastParams = {}
RaycastParams.__index = RaycastParams
function RaycastParams.new()
    return setmetatable({
        FilterDescendantsInstances = {},
        FilterType = 1, -- Exclude by default
        IgnoreWater = false,
        RespectCanCollide = false,
        MaxDistance = 1000,
    }, RaycastParams)
end
function RaycastParams:AddToFilter(...)
    for _, inst in ipairs({...}) do
        table.insert(self.FilterDescendantsInstances, inst)
    end
end

-- ══════════════════════════════════════════════════════════════════════
--  shared — tabla compartida entre scripts (equivalente a Roblox shared)
-- ══════════════════════════════════════════════════════════════════════
if not shared then shared = {} end

-- ══════════════════════════════════════════════════════════════════════
--  TweenInfo — tipo de dato para configurar animaciones
-- ══════════════════════════════════════════════════════════════════════
TweenInfo = {}
TweenInfo.__index = TweenInfo

function TweenInfo.new(tweenTime, easingStyle, easingDirection, repeatCount, reverses, delayTime)
    return setmetatable({
        Time            = tweenTime or 1,
        EasingStyle     = easingStyle or Enum.EasingStyle.Quad,
        EasingDirection = easingDirection or Enum.EasingDirection.Out,
        RepeatCount     = repeatCount or 0,
        Reverses        = reverses or false,
        DelayTime       = delayTime or 0,
    }, TweenInfo)
end
)LUAU"
R"LUAU(
-- ══════════════════════════════════════════════════════════════════════
--  TweenService — Roblox-style property interpolation
-- ////
--  TweenService — interpolación de propiedades estilo Roblox
-- ══════════════════════════════════════════════════════════════════════
local function _ease(styleEnum, dirEnum, t)
    if t <= 0 then return 0 elseif t >= 1 then return 1 end
    -- EasingStyle: Linear=0,Sine=1,Quad=2,Cubic=3,Quart=4,Quint=5,Bounce=6,Elastic=7,Back=8,Expo=9
    local s = styleEnum or 2  -- Quad por defecto

    local function easeIn(x)
        if s == 0 then return x                                                    -- Linear
        elseif s == 2 then return x * x                                            -- Quad
        elseif s == 3 then return x * x * x                                        -- Cubic
        elseif s == 4 then return x * x * x * x                                    -- Quart
        elseif s == 5 then return x * x * x * x * x                               -- Quint
        elseif s == 1 then return 1 - math.cos(x * math.pi * 0.5)                 -- Sine
        elseif s == 9 then return x == 0 and 0 or math.pow(2, 10 * x - 10)        -- Exponential
        elseif s == 8 then -- Back
            local c1 = 1.70158
            return (c1 + 1) * x * x * x - c1 * x * x
        elseif s == 6 then -- Bounce In = 1 - bounceOut(1-x)
            local bx = 1 - x
            local n1, d1 = 7.5625, 2.75
            local r
            if bx < 1 / d1 then r = n1 * bx * bx
            elseif bx < 2 / d1 then bx -= 1.5 / d1; r = n1 * bx * bx + 0.75
            elseif bx < 2.5 / d1 then bx -= 2.25 / d1; r = n1 * bx * bx + 0.9375
            else bx -= 2.625 / d1; r = n1 * bx * bx + 0.984375 end
            return 1 - r
        elseif s == 7 then -- Elastic
            local c4 = (2 * math.pi) / 3
            return x == 0 and 0 or -math.pow(2, 10 * x - 10) * math.sin((x * 10 - 10.75) * c4)
        end
        return x
    end

    local function easeOut(x)
        if s == 0 then return x
        elseif s == 2 then local v = 1 - x; return 1 - v * v           -- Quad
        elseif s == 3 then local v = 1 - x; return 1 - v * v * v       -- Cubic
        elseif s == 4 then local v = 1 - x; return 1 - v * v * v * v   -- Quart
        elseif s == 5 then local v = 1 - x; return 1 - v * v * v * v * v -- Quint
        elseif s == 1 then return math.sin(x * math.pi * 0.5)           -- Sine
        elseif s == 9 then return x == 1 and 1 or 1 - math.pow(2, -10 * x) -- Expo
        elseif s == 8 then -- Back
            local c1 = 1.70158; local c3 = c1 + 1; local v = x - 1
            return 1 + c3 * v * v * v + c1 * v * v
        elseif s == 6 then -- Bounce Out
            local n1, d1 = 7.5625, 2.75
            if x < 1 / d1 then return n1 * x * x
            elseif x < 2 / d1 then x -= 1.5 / d1; return n1 * x * x + 0.75
            elseif x < 2.5 / d1 then x -= 2.25 / d1; return n1 * x * x + 0.9375
            else x -= 2.625 / d1; return n1 * x * x + 0.984375 end
        elseif s == 7 then -- Elastic
            local c4 = (2 * math.pi) / 3
            return x == 1 and 1 or math.pow(2, -10 * x) * math.sin((x * 10 - 0.75) * c4) + 1
        end
        return x
    end

    local d = dirEnum or 1  -- Out por defecto
    if d == 0 then return easeIn(t)
    elseif d == 1 then return easeOut(t)
    else
        if t < 0.5 then return easeIn(t * 2) * 0.5
        else return 1 - easeIn((1 - t) * 2) * 0.5 end
    end
end

local function _lerp_val(a, b, t)
    local ta, tb = type(a), type(b)
    if ta == "number" and tb == "number" then
        return a + (b - a) * t
    end
    -- Vector3 interpolation: acceder componentes por nombre
    if ta == "userdata" and tb == "userdata" then
        local ax = rawget(a, "x") or 0; local ay = rawget(a, "y") or 0; local az = rawget(a, "z") or 0
        local bx = rawget(b, "x") or 0; local by = rawget(b, "y") or 0; local bz = rawget(b, "z") or 0
        return Vector3.new(ax + (bx - ax) * t, ay + (by - ay) * t, az + (bz - az) * t)
    end
    -- UDim2 / Color3 table interpolation
    if ta == "table" and tb == "table" then
        if a.X ~= nil and type(a.X) == "table" and a.X.Scale ~= nil then
            return UDim2.new(
                (a.X.Scale  or 0) + ((b.X.Scale  or 0) - (a.X.Scale  or 0)) * t,
                (a.X.Offset or 0) + ((b.X.Offset or 0) - (a.X.Offset or 0)) * t,
                (a.Y.Scale  or 0) + ((b.Y.Scale  or 0) - (a.Y.Scale  or 0)) * t,
                (a.Y.Offset or 0) + ((b.Y.Offset or 0) - (a.Y.Offset or 0)) * t
            )
        end
        if a.R ~= nil then
            return Color3.new(
                (a.R or 0) + ((b.R or 0) - (a.R or 0)) * t,
                (a.G or 0) + ((b.G or 0) - (a.G or 0)) * t,
                (a.B or 0) + ((b.B or 0) - (a.B or 0)) * t
            )
        end
        if a.r ~= nil then
            return Color3.new(
                (a.r or 0) + ((b.r or 0) - (a.r or 0)) * t,
                (a.g or 0) + ((b.g or 0) - (a.g or 0)) * t,
                (a.b or 0) + ((b.b or 0) - (a.b or 0)) * t
            )
        end
    end
    return t >= 1 and b or a
end

local TweenService = {}
TweenService.__index = TweenService

function TweenService:Create(instance, tweenInfo, goals)
    if not instance or not tweenInfo then return nil end

    local completedCbs = {}
    local tween = {
        PlaybackState = "Begin",
        Completed = {
            Connect = function(_, fn) table.insert(completedCbs, fn) end,
            Once    = function(_, fn)
                local conn; conn = { _fired = false }
                table.insert(completedCbs, function()
                    if not conn._fired then conn._fired = true; fn() end
                end)
            end,
        },
        _playing   = false,
        _cancelled = false,
        _conn      = nil,
    }

    function tween:Play()
        if self._playing then return end
        if self._conn then self._conn:Disconnect(); self._conn = nil end
        self._playing   = true
        self._cancelled = false
        self.PlaybackState = "Playing"
        local info = tweenInfo
        local gls  = goals

        local dur     = info.Time or 1
        local style   = info.EasingStyle or Enum.EasingStyle.Quad
        local dir     = info.EasingDirection or Enum.EasingDirection.Out
        local delay   = info.DelayTime or 0
        local repeats = info.RepeatCount or 0
        local rev     = info.Reverses or false

        local startVals = {}
        for prop, _ in pairs(gls) do
            local ok, v = pcall(function() return instance[prop] end)
            if ok then startVals[prop] = v end
        end

        local rs      = game:GetService("RunService")
        local rep     = 0
        local forward = true
        local startT  = time() + delay
        local conn

        conn = rs.Heartbeat:Connect(function(dt)
            if self._cancelled or not self._playing then
                conn:Disconnect(); self._conn = nil
                return
            end

            local now     = time()
            local elapsed = now - startT
            if elapsed < 0 then return end
            if elapsed > dur then elapsed = dur end

            local t     = dur > 0 and elapsed / dur or 1
            local eased = _ease(style, dir, forward and t or (1 - t))

            for prop, goal in pairs(gls) do
                local sv = startVals[prop]
                if sv ~= nil then
                    pcall(function() instance[prop] = _lerp_val(sv, goal, eased) end)
                end
            end

            if elapsed >= dur then
                if rev and forward then
                    forward = false
                    startT  = now
                else
                    rep += 1
                    if rep > repeats then
                        conn:Disconnect(); self._conn = nil
                        self._playing = false
                        self.PlaybackState = "Completed"
                        for _, fn in ipairs(completedCbs) do pcall(fn) end
                    else
                        forward = true
                        startT  = now
                    end
                end
            end
        end)
        self._conn = conn
    end

    function tween:Pause()
        if not self._playing then return end
        self._playing = false
        self.PlaybackState = "Paused"
        if self._conn then self._conn:Disconnect(); self._conn = nil end
    end

    function tween:Cancel()
        self._cancelled = true
        self._playing   = false
        self.PlaybackState = "Cancelled"
        if self._conn then self._conn:Disconnect(); self._conn = nil end
    end

    return tween
end

_G["__SVC_TweenService"] = TweenService
)LUAU"
R"LUAU(

-- ══════════════════════════════════════════════════════════════════════
--  Debris — automatically removes instances after N seconds
-- ////
--  Debris — elimina instancias automáticamente tras N segundos
-- ══════════════════════════════════════════════════════════════════════
local Debris = {}
Debris.__index = Debris

function Debris:AddItem(instance, lifetime)
    task.delay(lifetime or 5, function()
        if instance ~= nil then
            pcall(function() instance:Destroy() end)
        end
    end)
end

_G["__SVC_Debris"] = Debris

-- ══════════════════════════════════════════════════════════════════════
--  CollectionService — sistema de tags en instancias
-- ══════════════════════════════════════════════════════════════════════
local CollectionService = {}
CollectionService.__index = CollectionService
local _ccs_listeners = {}
local _ccs_tags      = {}   -- {[tag] = {[instance] = true}}
local _ccs_inst_tags = {}   -- {[instance] = {[tag] = true}}

function CollectionService:AddTag(instance, tag)
    pcall(function() instance:SetAttribute("__TAG_" .. tag, true) end)
    if not _ccs_tags[tag] then _ccs_tags[tag] = {} end
    _ccs_tags[tag][instance] = true
    if not _ccs_inst_tags[instance] then _ccs_inst_tags[instance] = {} end
    _ccs_inst_tags[instance][tag] = true
    if _ccs_listeners["InstanceTagged:" .. tag] then
        for _, fn in ipairs(_ccs_listeners["InstanceTagged:" .. tag]) do pcall(fn, instance) end
    end
end

function CollectionService:RemoveTag(instance, tag)
    pcall(function() instance:SetAttribute("__TAG_" .. tag, nil) end)
    if _ccs_tags[tag] then _ccs_tags[tag][instance] = nil end
    if _ccs_inst_tags[instance] then _ccs_inst_tags[instance][tag] = nil end
    if _ccs_listeners["InstanceUntagged:" .. tag] then
        for _, fn in ipairs(_ccs_listeners["InstanceUntagged:" .. tag]) do pcall(fn, instance) end
    end
end

function CollectionService:HasTag(instance, tag)
    return _ccs_inst_tags[instance] ~= nil and _ccs_inst_tags[instance][tag] == true
end

function CollectionService:GetTagged(tag)
    local result = {}
    if _ccs_tags[tag] then
        for inst in pairs(_ccs_tags[tag]) do table.insert(result, inst) end
    end
    return result
end

function CollectionService:GetTags(instance)
    local result = {}
    if _ccs_inst_tags[instance] then
        for tag in pairs(_ccs_inst_tags[instance]) do table.insert(result, tag) end
    end
    return result
end

function CollectionService:GetInstanceAddedSignal(tag)
    return {
        Connect = function(_, fn)
            _ccs_listeners["InstanceTagged:" .. tag] = _ccs_listeners["InstanceTagged:" .. tag] or {}
            table.insert(_ccs_listeners["InstanceTagged:" .. tag], fn)
        end
    }
end

function CollectionService:GetInstanceRemovedSignal(tag)
    return {
        Connect = function(_, fn)
            _ccs_listeners["InstanceUntagged:" .. tag] = _ccs_listeners["InstanceUntagged:" .. tag] or {}
            table.insert(_ccs_listeners["InstanceUntagged:" .. tag], fn)
        end
    }
end

_G["__SVC_CollectionService"] = CollectionService

-- ══════════════════════════════════════════════════════════════════════
--  DataStoreService — persistencia de datos (local + HTTP opcional)
-- ══════════════════════════════════════════════════════════════════════
local DataStoreService = {}
DataStoreService.__index = DataStoreService
local _ds_stores  = {}
local _ds_ordered = {}

local function _ds_safe_encode(v)
    if not _JSON then return tostring(v) end
    local ok, r = pcall(_JSON.encode, v)
    return ok and r or "{}"
end

local function _ds_safe_decode(s)
    if not s or s == "" then return nil end
    if not _JSON then return nil end
    local ok, r = pcall(_JSON.decode, s)
    return ok and r or nil
end

local function _ds_path(storeName, key)
    return "user://ds_" .. storeName .. "_" .. tostring(key) .. ".json"
end

local function _make_datastore(name)
    local store = { _name = name, _cache = {} }

    function store:GetAsync(key)
        local sk = tostring(key)
        if self._cache[sk] ~= nil then return self._cache[sk] end
        if _FileAccess then
            local content = _FileAccess.read(_ds_path(self._name, sk))
            local v = _ds_safe_decode(content)
            self._cache[sk] = v
            return v
        end
        return nil
    end

    function store:SetAsync(key, value)
        local sk = tostring(key)
        self._cache[sk] = value
        if _FileAccess then
            _FileAccess.write(_ds_path(self._name, sk), _ds_safe_encode(value))
        end
    end

    function store:UpdateAsync(key, transform)
        local old = self:GetAsync(key)
        local new_val = transform(old)
        if new_val ~= nil then self:SetAsync(key, new_val) end
        return new_val
    end

    function store:IncrementAsync(key, delta)
        local val = (self:GetAsync(key) or 0) + (delta or 1)
        self:SetAsync(key, val)
        return val
    end

    function store:RemoveAsync(key)
        local sk = tostring(key)
        local old = self._cache[sk]
        self._cache[sk] = nil
        if _FileAccess then
            pcall(function() _FileAccess.delete(_ds_path(self._name, sk)) end)
        end
        return old
    end

    function store:GetSortedAsync(ascending, pageSize, minValue, maxValue)
        return {
            GetCurrentPage = function() return {} end,
            AdvanceToNextPageAsync = function() end,
            IsFinished = true,
        }
    end

    return store
end

function DataStoreService:GetDataStore(name, scope)
    local key = scope and (name .. "_" .. scope) or name
    if not _ds_stores[key] then _ds_stores[key] = _make_datastore(key) end
    return _ds_stores[key]
end

function DataStoreService:GetOrderedDataStore(name, scope)
    local key = scope and (name .. "_" .. scope) or name
    if not _ds_ordered[key] then _ds_ordered[key] = _make_datastore("__ord_" .. key) end
    return _ds_ordered[key]
end

function DataStoreService:GetGlobalDataStore()
    return self:GetDataStore("__global")
end

function DataStoreService:GetRequestBudgetForRequestType(requestType)
    return 60
end

_G["__SVC_DataStoreService"] = DataStoreService
)LUAU"
R"LUAU(
-- ══════════════════════════════════════════════════════════════════════
--  HttpService — HTTP requests and JSON serialization
-- ////
--  HttpService — peticiones HTTP y serialización JSON
-- ══════════════════════════════════════════════════════════════════════
local HttpService = {}
HttpService.__index = HttpService
HttpService.HttpEnabled = true

function HttpService:JSONEncode(value)
    local ok, r = pcall(_JSON and _JSON.encode or function() return "{}" end, value)
    return ok and r or "{}"
end

function HttpService:JSONDecode(json)
    if not json or json == "" then return {} end
    local ok, r = pcall(_JSON and _JSON.decode or function() return {} end, json)
    return ok and r or {}
end

function HttpService:GetAsync(url, noCache, headers)
    if _HTTP then
        local ok, r = pcall(_HTTP.get, url)
        if ok then return r end
    end
    warn("[HttpService] GetAsync sin conexión: " .. tostring(url))
    return ""
end

function HttpService:PostAsync(url, data, contentType, compress, headers)
    if _HTTP then
        local ok, r = pcall(_HTTP.post, url, data or "", contentType or "application/json")
        if ok then return r end
    end
    warn("[HttpService] PostAsync sin conexión: " .. tostring(url))
    return ""
end

function HttpService:RequestAsync(options)
    local method = (options.Method or "GET"):upper()
    local url    = options.Url or ""
    local body   = options.Body or ""
    if method == "GET" then
        return { Success = true, StatusCode = 200, StatusMessage = "OK", Body = self:GetAsync(url) }
    elseif method == "POST" then
        return { Success = true, StatusCode = 200, StatusMessage = "OK", Body = self:PostAsync(url, body) }
    end
    return { Success = false, StatusCode = 0, StatusMessage = "Unsupported", Body = "" }
end

function HttpService:GenerateGUID(wrapInCurlyBraces)
    local t = math.floor(tick() * 1000)
    local r1, r2, r3, r4 = math.random(0, 0xFFFF), math.random(0x4000, 0x4FFF), math.random(0x8000, 0xBFFF), math.random(0, 0xFFFF)
    local guid = string.format("%08x-%04x-%04x-%04x-%04x%08x", t % 0xFFFFFFFF, r1, r2, r3, r4, t % 0xFFFFFFFF)
    return wrapInCurlyBraces and "{" .. guid .. "}" or guid
end

function HttpService:UrlEncode(input)
    return (tostring(input):gsub("[^%w%-_%.~]", function(c)
        return string.format("%%%02X", string.byte(c))
    end))
end

_G["__SVC_HttpService"] = HttpService

-- ══════════════════════════════════════════════════════════════════════
--  ContextActionService — binding de acciones a teclas
-- ══════════════════════════════════════════════════════════════════════
local ContextActionService = {}
ContextActionService.__index = ContextActionService
local _cas_actions = {}

function ContextActionService:BindAction(name, func, createButton, ...)
    _cas_actions[name] = { func = func, keys = {...}, button = createButton }
end

function ContextActionService:UnbindAction(name)
    _cas_actions[name] = nil
end

function ContextActionService:BindActionAtPriority(name, func, createButton, priority, ...)
    _cas_actions[name] = { func = func, keys = {...}, button = createButton, priority = priority }
end

function ContextActionService:GetBoundActionInfo(name)
    return _cas_actions[name]
end

function ContextActionService:GetAllBoundActionInfo()
    return _cas_actions
end

function ContextActionService:CallFunction(name, state, inputObject)
    local action = _cas_actions[name]
    if action and action.func then
        pcall(action.func, name, state, inputObject)
    end
end

function ContextActionService:SetTitle(name, title) end
function ContextActionService:SetImage(name, image) end
function ContextActionService:SetPosition(name, position) end

_G["__SVC_ContextActionService"] = ContextActionService

-- ══════════════════════════════════════════════════════════════════════
--  PathfindingService — agent navigation (functional stub)
-- ////
--  PathfindingService — navegación de agentes (stub funcional)
-- ══════════════════════════════════════════════════════════════════════
local PathfindingService = {}
PathfindingService.__index = PathfindingService

function PathfindingService:CreatePath(agentParameters)
    local blockedCbs = {}
    local path = {
        Status     = "NoPath",
        _waypoints = {},
        Blocked = {
            Connect = function(_, fn) table.insert(blockedCbs, fn) end,
        },
    }

    function path:ComputeAsync(startPos, goalPos)
        self._waypoints = {
            { Position = startPos, Action = "Walk" },
            { Position = goalPos,  Action = "Walk" },
        }
        self.Status = "Success"
    end

    function path:GetWaypoints()
        return self._waypoints
    end

    function path:CheckOcclusionAsync(startWaypointIndex)
        return -1
    end

    return path
end

_G["__SVC_PathfindingService"] = PathfindingService

-- ══════════════════════════════════════════════════════════════════════
--  PhysicsService — collision groups and filters
-- ////
--  PhysicsService — grupos y filtros de colisión
-- ══════════════════════════════════════════════════════════════════════
local PhysicsService = {}
PhysicsService.__index = PhysicsService
local _ps_groups = {}

function PhysicsService:RegisterCollisionGroup(name)
    if not _ps_groups[name] then _ps_groups[name] = { collidable = {} } end
end

function PhysicsService:CreateCollisionGroup(name)
    self:RegisterCollisionGroup(name)
end

function PhysicsService:RemoveCollisionGroup(name)
    _ps_groups[name] = nil
end

function PhysicsService:CollisionGroupSetCollidable(name1, name2, collidable)
    if not _ps_groups[name1] then self:RegisterCollisionGroup(name1) end
    if not _ps_groups[name2] then self:RegisterCollisionGroup(name2) end
    _ps_groups[name1].collidable[name2] = collidable
    _ps_groups[name2].collidable[name1] = collidable
end

function PhysicsService:CollisionGroupsAreCollidable(name1, name2)
    if _ps_groups[name1] and _ps_groups[name1].collidable[name2] == false then return false end
    return true
end

function PhysicsService:SetPartCollisionGroup(part, groupName)
    pcall(function() part:SetAttribute("__CollGroup", groupName) end)
end

function PhysicsService:GetPartCollisionGroup(part)
    local ok, v = pcall(function() return part:GetAttribute("__CollGroup") end)
    return (ok and v) or "Default"
end

function PhysicsService:GetCollisionGroupId(name)
    local i = 0
    for k, _ in pairs(_ps_groups) do
        i += 1
        if k == name then return i end
    end
    return 0
end

function PhysicsService:GetCollisionGroupName(id)
    local i = 0
    for k, _ in pairs(_ps_groups) do
        i += 1
        if i == id then return k end
    end
    return "Default"
end

_G["__SVC_PhysicsService"] = PhysicsService
)LUAU"
R"LUAU(
-- ══════════════════════════════════════════════════════════════════════
--  TeleportService — teletransporte entre lugares (stub)
-- ══════════════════════════════════════════════════════════════════════
local TeleportService = {}
TeleportService.__index = TeleportService
local _ts_callbacks = {}

TeleportService.TeleportInitFailed = { Connect = function(_, fn) end }
TeleportService.LocalPlayerArrivedFromTeleport = { Connect = function(_, fn) end }

function TeleportService:Teleport(placeId, player, teleportData, customLoadingScreen)
    warn("[TeleportService] Teleport a " .. tostring(placeId) .. " no soportado en GodotLuau.")
end

function TeleportService:TeleportAsync(placeId, players, teleportOptions)
    warn("[TeleportService] TeleportAsync no soportado.")
    return { Instances = players or {} }
end

function TeleportService:TeleportToSpawnByName(placeId, spawnName, player, teleportData)
    warn("[TeleportService] TeleportToSpawnByName no soportado.")
end

function TeleportService:TeleportToPlaceInstance(placeId, instanceId, player)
    warn("[TeleportService] TeleportToPlaceInstance no soportado.")
end

function TeleportService:TeleportPartyAsync(placeId, players, teleportOptions)
    warn("[TeleportService] TeleportPartyAsync no soportado.")
    return {}
end

function TeleportService:GetLocalPlayerTeleportData()
    return nil
end

function TeleportService:GetTeleportSetting(name)
    return nil
end

function TeleportService:SetTeleportSetting(name, value) end
function TeleportService:SetTeleportGui(gui) end

_G["__SVC_TeleportService"] = TeleportService

)LUAU"
// Segunda mitad del stdlib — MSVC no permite string literals > 65535 chars
R"LUAU(
-- ══════════════════════════════════════════════════════════════════════
--  BadgeService — sistema de medallas (stub con logging)
-- ══════════════════════════════════════════════════════════════════════
local BadgeService = {}
BadgeService.__index = BadgeService

function BadgeService:AwardBadge(userId, badgeId)
    print("[BadgeService] AwardBadge:", userId, badgeId, "— en juego real se enviaría al servidor.")
    return true
end

function BadgeService:UserHasBadgeAsync(userId, badgeId)
    return false
end

function BadgeService:GetBadgeInfoAsync(badgeId)
    return {
        Name        = "Badge " .. tostring(badgeId),
        Description = "",
        IsEnabled   = true,
        Points      = 0,
        IconImageId = 0,
    }
end

_G["__SVC_BadgeService"] = BadgeService

-- ══════════════════════════════════════════════════════════════════════
--  MarketplaceService — compras en juego (stub)
-- ══════════════════════════════════════════════════════════════════════
local MarketplaceService = {}
MarketplaceService.__index = MarketplaceService
local _mps_receipts = {}

MarketplaceService.ProcessReceipt = nil
MarketplaceService.PromptProductPurchaseFinished = { Connect = function(_, fn) end }
MarketplaceService.PromptGamePassPurchaseFinished = { Connect = function(_, fn) end }
MarketplaceService.PromptPurchaseFinished = { Connect = function(_, fn) end }

function MarketplaceService:PromptProductPurchase(player, productId, equipIfPurchased, currencyType)
    warn("[MarketplaceService] PromptProductPurchase no soportado en modo offline.")
end

function MarketplaceService:PromptGamePassPurchase(player, gamePassId)
    warn("[MarketplaceService] PromptGamePassPurchase no soportado en modo offline.")
end

function MarketplaceService:PromptPurchase(player, assetId, equipIfPurchased, currencyType)
    warn("[MarketplaceService] PromptPurchase no soportado en modo offline.")
end

function MarketplaceService:UserOwnsGamePassAsync(userId, gamePassId)
    return false
end

function MarketplaceService:PlayerOwnsAsset(player, assetId)
    return false
end

function MarketplaceService:GetProductInfo(assetId, infoType)
    return {
        Name          = "Product " .. tostring(assetId),
        Description   = "",
        PriceInRobux  = 0,
        AssetId       = assetId,
        IsPublicDomain = false,
        IsForSale     = false,
    }
end

_G["__SVC_MarketplaceService"] = MarketplaceService
)LUAU"
R"LUAU(

-- ══════════════════════════════════════════════════════════════════════
--  GuiService — user interface management
-- ////
--  GuiService — gestión de interfaces de usuario
-- ══════════════════════════════════════════════════════════════════════
local GuiService = {}
GuiService.__index = GuiService

GuiService.MenuOpened  = { Connect = function() end }
GuiService.MenuClosed  = { Connect = function() end }
GuiService.ErrorMessageChanged = { Connect = function() end }

function GuiService:GetScreenResolution()
    return Vector3.new(1920, 1080, 0)
end

function GuiService:SetMenuIsOpen(open) end
function GuiService:GetInspectMenuEnabled() return false end
function GuiService:GetNotificationTypeList() return {} end
function GuiService:CloseInspectMenu() end
function GuiService:InspectPlayerFromHumanoidDescription(humanoidDescription, name) end
function GuiService:SetGlobalGuiInset(inset) end
function GuiService:GetGlobalGuiInset() return 0, 0, 0, 0 end
function GuiService:IsTenFootInterface() return false end
function GuiService:IsKeyboardGamepadNavigationEnabled() return false end

_G["__SVC_GuiService"] = GuiService

-- ══════════════════════════════════════════════════════════════════════
--  InsertService — external asset loader (stub)
-- ////
--  InsertService — carga de assets externos (stub)
-- ══════════════════════════════════════════════════════════════════════
local InsertService = {}
InsertService.__index = InsertService

function InsertService:LoadAsset(assetId)
    warn("[InsertService] LoadAsset(" .. tostring(assetId) .. ") no soportado en GodotLuau.")
    return nil
end

function InsertService:LoadAssetVersion(assetVersionId)
    warn("[InsertService] LoadAssetVersion no soportado.")
    return nil
end

function InsertService:GetFreeDecals(searchText, pageNum) return {} end
function InsertService:GetFreeModels(searchText, pageNum) return {} end
function InsertService:GetProductInfo(assetId) return {} end

_G["__SVC_InsertService"] = InsertService

-- ══════════════════════════════════════════════════════════════════════
--  ScriptContext — script execution context
-- ////
--  ScriptContext — contexto de ejecución de scripts
-- ══════════════════════════════════════════════════════════════════════
local ScriptContext = {}
ScriptContext.__index = ScriptContext

ScriptContext.Error = {
    Connect = function(_, fn)
        -- Could be hooked to the global error handler in the future / Podría hookearse al error handler global en el futuro
    end
}

function ScriptContext:GetCurrent() return nil end
function ScriptContext:AddCoreScriptLocal(name, parent) end

_G["__SVC_ScriptContext"] = ScriptContext

-- ══════════════════════════════════════════════════════════════════════
--  Vector2 — 2D vector (UI coordinates, input, etc.)
-- ////
--  Vector2 — vector 2D (coordenadas de UI, input, etc.)
-- ══════════════════════════════════════════════════════════════════════
Vector2 = {}
Vector2.__index = Vector2

function Vector2.new(x, y)
    return setmetatable({ X = x or 0, Y = y or 0, x = x or 0, y = y or 0 }, Vector2)
end
function Vector2.__tostring(v) return "(" .. v.X .. ", " .. v.Y .. ")" end
function Vector2.__add(a, b)   return Vector2.new(a.X + b.X, a.Y + b.Y) end
function Vector2.__sub(a, b)   return Vector2.new(a.X - b.X, a.Y - b.Y) end
function Vector2.__mul(a, b)
    if type(b) == "number" then return Vector2.new(a.X * b, a.Y * b) end
    return Vector2.new(a.X * b.X, a.Y * b.Y)
end
function Vector2:magnitude()  return math.sqrt(self.X * self.X + self.Y * self.Y) end
function Vector2:normalized() local m = self:magnitude(); if m < 0.0001 then return Vector2.new(0,0) end; return Vector2.new(self.X/m, self.Y/m) end
function Vector2:dot(v2)      return self.X * v2.X + self.Y * v2.Y end
function Vector2:lerp(v2, t)  return Vector2.new(self.X + (v2.X - self.X)*t, self.Y + (v2.Y - self.Y)*t) end
Vector2.zero = Vector2.new(0, 0)
Vector2.one  = Vector2.new(1, 1)

-- ══════════════════════════════════════════════════════════════════════
--  UDim — single dimension (scale + offset)
-- ////
--  UDim — dimensión única (scale + offset)
-- ══════════════════════════════════════════════════════════════════════
UDim = {}
UDim.__index = UDim
function UDim.new(scale, offset)
    return setmetatable({ Scale = scale or 0, Offset = offset or 0 }, UDim)
end

-- ══════════════════════════════════════════════════════════════════════
--  UDim2 — Roblox-style 2D position/size
--  Usage:  UDim2.new(xScale, xOffset, yScale, yOffset)
--          UDim2.fromScale(xS, yS)
--          UDim2.fromOffset(xO, yO)
--  Access: size.X.Scale, size.X.Offset, size.Y.Scale, size.Y.Offset
-- ////
--  UDim2 — posición/tamaño bidimensional tipo Roblox
--  Uso: UDim2.new(xScale, xOffset, yScale, yOffset)
--       UDim2.fromScale(xS, yS)
--       UDim2.fromOffset(xO, yO)
--  Acceso: size.X.Scale, size.X.Offset, size.Y.Scale, size.Y.Offset
-- ══════════════════════════════════════════════════════════════════════
UDim2 = {}
UDim2.__index = UDim2

function UDim2.new(xScale, xOffset, yScale, yOffset)
    local t = {
        X = UDim.new(xScale or 0, xOffset or 0),
        Y = UDim.new(yScale or 0, yOffset or 0),
    }
    t.Width  = t.X
    t.Height = t.Y
    return setmetatable(t, UDim2)
end

function UDim2.fromScale(xScale, yScale)
    return UDim2.new(xScale, 0, yScale, 0)
end

function UDim2.fromOffset(xOffset, yOffset)
    return UDim2.new(0, xOffset, 0, yOffset)
end

function UDim2.__add(a, b)
    return UDim2.new(a.X.Scale + b.X.Scale, a.X.Offset + b.X.Offset,
                     a.Y.Scale + b.Y.Scale, a.Y.Offset + b.Y.Offset)
end

function UDim2.__sub(a, b)
    return UDim2.new(a.X.Scale - b.X.Scale, a.X.Offset - b.X.Offset,
                     a.Y.Scale - b.Y.Scale, a.Y.Offset - b.Y.Offset)
end

function UDim2:lerp(b, t)
    return UDim2.new(
        self.X.Scale  + (b.X.Scale  - self.X.Scale)  * t,
        self.X.Offset + (b.X.Offset - self.X.Offset) * t,
        self.Y.Scale  + (b.Y.Scale  - self.Y.Scale)  * t,
        self.Y.Offset + (b.Y.Offset - self.Y.Offset) * t
    )
end

function UDim2.__tostring(u)
    return string.format("{%g,%g},{%g,%g}", u.X.Scale, u.X.Offset, u.Y.Scale, u.Y.Offset)
end

-- ══════════════════════════════════════════════════════════════════════
--  CFrame — Roblox-style 3D transformation system
--  Stores position + 3x3 rotation matrix (same as Roblox)
--  CFrame.new(x,y,z)                     — position, no rotation
--  CFrame.new(pos, lookAt)               — look toward a Vector3
--  CFrame.new(x,y,z, r00,r01,r02,...)   — position + 9 rotation components
--  CFrame.fromEulerAnglesXYZ(rx,ry,rz)  — from Euler angles
--  CFrame.Angles(rx,ry,rz)              — alias for fromEulerAnglesXYZ
--  CFrame.lookAt(from, to)              — equivalent to CFrame.new(from, to)
--  cf * cf2                             — multiply (compose transforms)
--  cf + v3                              — translate
--  cf:Inverse()                         — inverse
--  cf:Lerp(cf2, t)                      — interpolation
--  cf:ToEulerAnglesXYZ()                — extract Euler angles
--  cf.Position / cf.X / cf.Y / cf.Z    — position
--  cf.LookVector / cf.RightVector / cf.UpVector — direction vectors
-- ////
--  CFrame — sistema de transformación 3D estilo Roblox
--  Almacena posición + matriz de rotación 3x3 (igual que Roblox)
--  CFrame.new(x,y,z)                     — posición, sin rotación
--  CFrame.new(pos, lookAt)               — apuntar hacia un Vector3
--  CFrame.new(x,y,z, r00,r01,r02,...)   — posición + 9 componentes de rotación
--  CFrame.fromEulerAnglesXYZ(rx,ry,rz)  — desde ángulos de Euler
--  CFrame.Angles(rx,ry,rz)              — alias de fromEulerAnglesXYZ
--  CFrame.lookAt(from, to)              — equivalente a CFrame.new(from, to)
--  cf * cf2                             — multiplicar (componer transformaciones)
--  cf + v3                              — trasladar
--  cf:Inverse()                         — inversa
--  cf:Lerp(cf2, t)                      — interpolación
--  cf:ToEulerAnglesXYZ()                — extraer ángulos de Euler
--  cf.Position / cf.X / cf.Y / cf.Z    — posición
--  cf.LookVector / cf.RightVector / cf.UpVector — vectores de dirección
-- ══════════════════════════════════════════════════════════════════════
)LUAU"
R"LUAU(
CFrame = {}
CFrame.__index = CFrame

local function _cf_raw(x,y,z, m00,m01,m02, m10,m11,m12, m20,m21,m22)
    local cf = {
        X=x,Y=y,Z=z,
        m00=m00,m01=m01,m02=m02,
        m10=m10,m11=m11,m12=m12,
        m20=m20,m21=m21,m22=m22,
    }
    cf.Position = { X=x, Y=y, Z=z, x=x, y=y, z=z }
    cf.RightVector    = { X=m00, Y=m10, Z=m20, x=m00, y=m10, z=m20 }
    cf.UpVector       = { X=m01, Y=m11, Z=m21, x=m01, y=m11, z=m21 }
    cf.LookVector     = { X=-m02, Y=-m12, Z=-m22, x=-m02, y=-m12, z=-m22 }
    return setmetatable(cf, CFrame)
end

function CFrame.new(ax, ay, az, bx, by, bz, m01, m02, m10, m11, m12, m20, m21, m22)
    if ax == nil then
        return _cf_raw(0,0,0, 1,0,0, 0,1,0, 0,0,1)
    end
    -- CFrame.new(Vector3_pos, Vector3_lookAt)
    if type(ax) == "table" and ay ~= nil and type(ay) == "table" then
        local px,py,pz = ax.X or ax.x or 0, ax.Y or ax.y or 0, ax.Z or ax.z or 0
        local lx,ly,lz = ay.X or ay.x or 0, ay.Y or ay.y or 0, ay.Z or ay.z or 0
        local fx,fy,fz = lx-px, ly-py, lz-pz
        local ln = math.sqrt(fx*fx+fy*fy+fz*fz)
        if ln < 1e-6 then return _cf_raw(px,py,pz, 1,0,0, 0,1,0, 0,0,1) end
        fx,fy,fz = fx/ln, fy/ln, fz/ln
        local upx,upy,upz = 0,1,0
        local rx = upy*fz - upz*fy
        local ry = upz*fx - upx*fz
        local rz = upx*fy - upy*fx
        local rn = math.sqrt(rx*rx+ry*ry+rz*rz)
        if rn < 1e-6 then rx,ry,rz = 1,0,0 else rx,ry,rz = rx/rn, ry/rn, rz/rn end
        local ux = fy*rz - fz*ry
        local uy = fz*rx - fx*rz
        local uz = fx*ry - fy*rx
        return _cf_raw(px,py,pz, rx,ux,-fx, ry,uy,-fy, rz,uz,-fz)
    end
    -- CFrame.new(x,y,z) — position only / posición sola
    if m01 == nil then
        ax = ax or 0; ay = ay or 0; az = az or 0
        return _cf_raw(ax,ay,az, 1,0,0, 0,1,0, 0,0,1)
    end
    -- CFrame.new(x,y,z, r00,r01,r02, r10,r11,r12, r20,r21,r22)
    return _cf_raw(ax,ay,az, bx,by,bz, m01,m02,m10, m11,m12,m20)
end

function CFrame.fromEulerAnglesXYZ(rx, ry, rz)
    local cx,sx = math.cos(rx), math.sin(rx)
    local cy,sy = math.cos(ry), math.sin(ry)
    local cz,sz = math.cos(rz), math.sin(rz)
    return _cf_raw(0,0,0,
        cy*cz,            -cy*sz,           sy,
        sx*sy*cz+cx*sz,   -sx*sy*sz+cx*cz,  -sx*cy,
        -cx*sy*cz+sx*sz,  cx*sy*sz+sx*cz,   cx*cy)
end
CFrame.Angles = CFrame.fromEulerAnglesXYZ
CFrame.fromEulerAnglesYXZ = function(ry,rx,rz)
    local cx,sx = math.cos(rx), math.sin(rx)
    local cy,sy = math.cos(ry), math.sin(ry)
    local cz,sz = math.cos(rz), math.sin(rz)
    return _cf_raw(0,0,0,
        cy*cz+sy*sx*sz,   cz*sy*sx-cy*sz,   cx*sy,
        cx*sz,            cx*cz,             -sx,
        cy*sx*sz-cz*sy,   sy*sz+cy*cz*sx,   cy*cx)
end

function CFrame.lookAt(from, to, up)
    return CFrame.new(from, to)
end

function CFrame.__mul(a, b)
    if type(b) == "table" and b.m00 ~= nil then
        -- CFrame * CFrame
        local x = a.X + a.m00*b.X + a.m01*b.Y + a.m02*b.Z
        local y = a.Y + a.m10*b.X + a.m11*b.Y + a.m12*b.Z
        local z = a.Z + a.m20*b.X + a.m21*b.Y + a.m22*b.Z
        return _cf_raw(x,y,z,
            a.m00*b.m00+a.m01*b.m10+a.m02*b.m20,
            a.m00*b.m01+a.m01*b.m11+a.m02*b.m21,
            a.m00*b.m02+a.m01*b.m12+a.m02*b.m22,
            a.m10*b.m00+a.m11*b.m10+a.m12*b.m20,
            a.m10*b.m01+a.m11*b.m11+a.m12*b.m21,
            a.m10*b.m02+a.m11*b.m12+a.m12*b.m22,
            a.m20*b.m00+a.m21*b.m10+a.m22*b.m20,
            a.m20*b.m01+a.m21*b.m11+a.m22*b.m21,
            a.m20*b.m02+a.m21*b.m12+a.m22*b.m22)
    end
    -- CFrame * Vector3
    local vx = b.X or b.x or 0
    local vy = b.Y or b.y or 0
    local vz = b.Z or b.z or 0
    local rx = a.X + a.m00*vx + a.m01*vy + a.m02*vz
    local ry = a.Y + a.m10*vx + a.m11*vy + a.m12*vz
    local rz = a.Z + a.m20*vx + a.m21*vy + a.m22*vz
    return { X=rx, Y=ry, Z=rz, x=rx, y=ry, z=rz }
end

function CFrame.__add(a, b)
    local bx = b.X or b.x or 0
    local by = b.Y or b.y or 0
    local bz = b.Z or b.z or 0
    return _cf_raw(a.X+bx, a.Y+by, a.Z+bz,
        a.m00,a.m01,a.m02, a.m10,a.m11,a.m12, a.m20,a.m21,a.m22)
end

function CFrame.__sub(a, b)
    local bx = b.X or b.x or 0
    local by = b.Y or b.y or 0
    local bz = b.Z or b.z or 0
    return _cf_raw(a.X-bx, a.Y-by, a.Z-bz,
        a.m00,a.m01,a.m02, a.m10,a.m11,a.m12, a.m20,a.m21,a.m22)
end

function CFrame.__eq(a, b)
    return a.X==b.X and a.Y==b.Y and a.Z==b.Z
       and a.m00==b.m00 and a.m11==b.m11 and a.m22==b.m22
end

function CFrame:Inverse()
    -- Transponer la parte de rotación (es ortogonal)
    local ix = -(self.m00*self.X + self.m10*self.Y + self.m20*self.Z)
    local iy = -(self.m01*self.X + self.m11*self.Y + self.m21*self.Z)
    local iz = -(self.m02*self.X + self.m12*self.Y + self.m22*self.Z)
    return _cf_raw(ix,iy,iz,
        self.m00,self.m10,self.m20,
        self.m01,self.m11,self.m21,
        self.m02,self.m12,self.m22)
end

function CFrame:Lerp(cf2, t)
    local x = self.X + (cf2.X - self.X) * t
    local y = self.Y + (cf2.Y - self.Y) * t
    local z = self.Z + (cf2.Z - self.Z) * t
    local function lm(a, b) return a + (b - a) * t end
    return _cf_raw(x,y,z,
        lm(self.m00,cf2.m00), lm(self.m01,cf2.m01), lm(self.m02,cf2.m02),
        lm(self.m10,cf2.m10), lm(self.m11,cf2.m11), lm(self.m12,cf2.m12),
        lm(self.m20,cf2.m20), lm(self.m21,cf2.m21), lm(self.m22,cf2.m22))
end

function CFrame:ToEulerAnglesXYZ()
    local sy = self.m02
    sy = math.max(-1, math.min(1, sy))
    local ry = math.asin(sy)
    local rx, rz
    if math.abs(sy) < 0.9999 then
        rx = math.atan2(-self.m12, self.m22)
        rz = math.atan2(-self.m01, self.m00)
    else
        rx = math.atan2(self.m21, self.m11)
        rz = 0
    end
    return rx, ry, rz
end
CFrame.ToOrientation = CFrame.ToEulerAnglesXYZ

function CFrame:GetComponents()
    return self.X,self.Y,self.Z,
           self.m00,self.m01,self.m02,
           self.m10,self.m11,self.m12,
           self.m20,self.m21,self.m22
end

function CFrame:ToWorldSpace(cf)
    return self * cf
end

function CFrame:ToObjectSpace(cf)
    return self:Inverse() * cf
end

function CFrame:PointToWorldSpace(v)
    return self * v
end

function CFrame:PointToObjectSpace(v)
    return self:Inverse() * v
end

function CFrame:VectorToWorldSpace(v)
    local vx = v.X or v.x or 0
    local vy = v.Y or v.y or 0
    local vz = v.Z or v.z or 0
    local rx = self.m00*vx + self.m01*vy + self.m02*vz
    local ry = self.m10*vx + self.m11*vy + self.m12*vz
    local rz = self.m20*vx + self.m21*vy + self.m22*vz
    return { X=rx, Y=ry, Z=rz, x=rx, y=ry, z=rz }
end

function CFrame:VectorToObjectSpace(v)
    return self:Inverse():VectorToWorldSpace(v)
end

function CFrame.__tostring(cf)
    return string.format("%.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f",
        cf.X,cf.Y,cf.Z, cf.m00,cf.m01,cf.m02, cf.m10,cf.m11,cf.m12, cf.m20,cf.m21,cf.m22)
end

CFrame.identity = _cf_raw(0,0,0, 1,0,0, 0,1,0, 0,0,1)

)LUAU"
// Third part of the stdlib / Tercera parte del stdlib
R"LUAU(
-- ══════════════════════════════════════════════════════════════════════
--  Instance.new — create instances at runtime
--  Saves the C++ version (which has the Part→RobloxPart mapping, etc.)
--  and uses it as first option; _InstanceNew as generic fallback.
-- ////
--  Instance.new — crear instancias en runtime
--  Guarda la versión C++ (que tiene el mapeo Part→RobloxPart etc.)
--  y la usa como primera opción; _InstanceNew como fallback genérico.
-- ══════════════════════════════════════════════════════════════════════
local _cpp_new = Instance and Instance.new or nil

-- Mapeo de nombres Roblox → nombres de clase Godot registrados
local _cls_map = {
    Part="RobloxPart", BasePart="RobloxPart", MeshPart="RobloxPart",
    WedgePart="RobloxPart", CornerWedgePart="RobloxPart", TrussPart="RobloxPart",
    UnionOperation="RobloxPart", SpecialMesh="RobloxPart",
    Folder="Folder", Model="Node3D", Configuration="Folder",
    Humanoid="Humanoid", Humanoid2D="Humanoid2D",
    PointLight="OmniLight3D", SpotLight="SpotLight3D",
    DirectionalLight="DirectionalLight3D", SurfaceLight="OmniLight3D",
    RemoteEvent="RemoteEventNode", RemoteFunction="RemoteFunctionNode",
    BindableEvent="BindableEventNode", BindableFunction="BindableEventNode",
    Script="ServerScript", LocalScript="LocalScript", ModuleScript="ModuleScript",
    -- Objetos de valor (se representan como Folder genérico con metadatos)
    StringValue="Folder", IntValue="Folder", NumberValue="Folder",
    BoolValue="Folder", ObjectValue="Folder", Vector3Value="Folder",
    Color3Value="Folder", CFrameValue="Folder",
    -- UI tipo Roblox
    ScreenGui="ScreenGui", Frame="RobloxFrame", TextLabel="RobloxTextLabel",
    TextButton="RobloxTextButton", TextBox="RobloxTextBox",
    ImageLabel="RobloxImageLabel", ImageButton="RobloxImageLabel",
    ScrollingFrame="RobloxScrollingFrame", LocalizationTable="Folder",
    -- BodyMovers
    BodyVelocity="BodyVelocity", BodyPosition="BodyPosition",
    BodyForce="BodyForce", BodyAngularVelocity="BodyAngularVelocity",
    BodyGyro="BodyGyro",
    -- Constraints
    WeldConstraint="WeldConstraint", HingeConstraint="HingeConstraint",
    BallSocketConstraint="BallSocketConstraint", RodConstraint="RodConstraint",
    SpringConstraint="SpringConstraint",
    -- Sound
    Sound="RobloxSound", SoundGroup="RobloxSoundGroup",
    -- Tween
    Tween="RobloxTween",
    -- Interactive
    ClickDetector="ClickDetector", ProximityPrompt="ProximityPrompt",
    SpawnLocation="SpawnLocation",
    -- Billboard / Surface GUI
    BillboardGui="BillboardGui", SurfaceGui="SurfaceGui",
    -- Motor6D / Tool / Backpack
    Motor6D="Motor6D", Tool="RobloxTool", Backpack="Backpack",
    -- Animation
    Animation="AnimationObject", AnimationTrack="AnimationTrack",
    -- Value objects
    NumberValue="Folder", StringValue="Folder", IntValue="Folder",
    BoolValue="Folder", ObjectValue="Folder", Vector3Value="Folder",
    Color3Value="Folder", CFrameValue="Folder",
}

Instance = Instance or {}
Instance.new = function(className, parent)
    local inst
    -- 1. Intentar con la función C++ original (mapeos hardcodeados)
    if _cpp_new then
        local ok, result = pcall(_cpp_new, className)
        if ok and result ~= nil then inst = result end
    end
    -- 2. Fallback: _InstanceNew con mapeo de nombres
    if inst == nil and _InstanceNew then
        local godotName = _cls_map[className] or className
        inst = _InstanceNew(godotName)
    end
    if inst == nil then
        warn("[Instance.new] Clase no encontrada: '" .. tostring(className) .. "'")
        return nil
    end
    -- Nombrar la instancia con el className de Roblox
    pcall(function() inst.Name = className end)
    -- Aplicar parent si se especificó
    if parent ~= nil then
        pcall(function() inst.Parent = parent end)
    end
    return inst
end

-- Instance.fromExisting: envuelve un nodo Godot existente (uso interno)
Instance.fromExisting = function(node)
    return node  -- los nodos ya vienen envueltos como GodotObject
end

)LUAU"
R"LUAU(
-- ══════════════════════════════════════════════════════════════════════
--  Value Objects — NumberValue, StringValue, BoolValue, etc. con Changed
--  Uso: local v = Instance.new("NumberValue")
--       v.Value = 42
--       v.Changed:Connect(function(newVal) end)
-- ══════════════════════════════════════════════════════════════════════
local function _make_value_object(className, defaultVal)
    local obj = {
        _value = defaultVal,
        _cbs   = {},
        Name   = className,
        ClassName = className,
    }
    obj.Changed = {
        Connect = function(_, fn)
            table.insert(obj._cbs, fn)
            -- Retornar conexión desconectable
            return {
                Disconnect = function()
                    for i, f in ipairs(obj._cbs) do
                        if f == fn then table.remove(obj._cbs, i); break end
                    end
                end
            }
        end
    }
    local mt = {}
    mt.__index = function(t, k)
        if k == "Value" then return t._value end
        return rawget(t, k)
    end
    mt.__newindex = function(t, k, v)
        if k == "Value" then
            local old = t._value
            rawset(t, "_value", v)
            if old ~= v then
                for _, fn in ipairs(t._cbs) do
                    local ok, err = pcall(fn, v)
                    if not ok then warn("[ValueObject] Changed error:", err) end
                end
            end
        else
            rawset(t, k, v)
        end
    end
    return setmetatable(obj, mt)
end

-- Parchar Instance.new para crear value objects reales
local _inst_new_prev = Instance.new
Instance.new = function(className, parent)
    local valueDefaults = {
        NumberValue = 0, IntValue = 0, BoolValue = false,
        StringValue = "", ObjectValue = nil,
        Vector3Value = nil, Color3Value = nil, CFrameValue = nil,
    }
    if valueDefaults[className] ~= nil then
        local obj = _make_value_object(className, valueDefaults[className])
        if parent ~= nil then
            pcall(function() obj.Parent = parent end)
        end
        return obj
    end
    return _inst_new_prev(className, parent)
end

-- ══════════════════════════════════════════════════════════════════════
--  Color3 extras — missing methods
-- ////
--  Color3 extras — métodos faltantes
-- ══════════════════════════════════════════════════════════════════════
if Color3 then
    Color3.fromHSV = Color3.fromHSV or function(h, s, v)
        h = h % 1
        if s == 0 then return Color3.new(v, v, v) end
        local i = math.floor(h * 6)
        local f = h * 6 - i
        local p, q, t2 = v*(1-s), v*(1-s*f), v*(1-s*(1-f))
        local r, g, b
        i = i % 6
        if i == 0 then r,g,b=v,t2,p elseif i == 1 then r,g,b=q,v,p
        elseif i == 2 then r,g,b=p,v,t2 elseif i == 3 then r,g,b=p,q,v
        elseif i == 4 then r,g,b=t2,p,v else r,g,b=v,p,q end
        return Color3.new(r, g, b)
    end
    Color3.toHSV = Color3.toHSV or function(c)
        local r,g,b = c.R or c.r or 0, c.G or c.g or 0, c.B or c.b or 0
        local max = math.max(r,g,b); local min = math.min(r,g,b); local delta = max - min
        local h = 0
        if delta > 0 then
            if max == r then h = (g-b)/delta % 6
            elseif max == g then h = (b-r)/delta + 2
            else h = (r-g)/delta + 4 end
            h = h / 6
        end
        return h, (max==0 and 0 or delta/max), max
    end
    Color3.lerp = Color3.lerp or function(c1, c2, t)
        local r1,g1,b1 = c1.R or c1.r or 0, c1.G or c1.g or 0, c1.B or c1.b or 0
        local r2,g2,b2 = c2.R or c2.r or 0, c2.G or c2.g or 0, c2.B or c2.b or 0
        return Color3.new(r1+(r2-r1)*t, g1+(g2-g1)*t, b1+(b2-b1)*t)
    end
    Color3.fromHex = Color3.fromHex or function(hex)
        hex = tostring(hex):gsub("^#", "")
        local r = tonumber(hex:sub(1,2), 16) or 0
        local g = tonumber(hex:sub(3,4), 16) or 0
        local b = tonumber(hex:sub(5,6), 16) or 0
        return Color3.new(r/255, g/255, b/255)
    end
    Color3.toHex = Color3.toHex or function(c)
        local r = math.max(0,math.min(255, math.floor((c.R or c.r or 0)*255+0.5)))
        local g = math.max(0,math.min(255, math.floor((c.G or c.g or 0)*255+0.5)))
        local b = math.max(0,math.min(255, math.floor((c.B or c.b or 0)*255+0.5)))
        return string.format("%02X%02X%02X", r, g, b)
    end
end

)LUAU"
// Fourth part of the stdlib / Cuarta parte del stdlib
R"LUAU(
-- ══════════════════════════════════════════════════════════════════════
--  Vector3 extras — Magnitude, Unit, Cross, Dot, Lerp as methods
-- ////
--  Vector3 extras — Magnitude, Unit, Cross, Dot, Lerp como métodos
-- ══════════════════════════════════════════════════════════════════════
if Vector3 then
    Vector3.zero  = Vector3.zero  or Vector3.new(0,0,0)
    Vector3.one   = Vector3.one   or Vector3.new(1,1,1)
    Vector3.xAxis = Vector3.xAxis or Vector3.new(1,0,0)
    Vector3.yAxis = Vector3.yAxis or Vector3.new(0,1,0)
    Vector3.zAxis = Vector3.zAxis or Vector3.new(0,0,1)
end

-- ══════════════════════════════════════════════════════════════════════
--  Random — equivalent to Roblox Random
-- ////
--  Random — equivalente a Roblox Random
-- ══════════════════════════════════════════════════════════════════════
Random = {}
Random.__index = Random
function Random.new(seed)
    local state = { _seed = seed or os.clock() }
    local mt = {}
    mt.__index = {
        NextNumber = function(self, min, max)
            self._seed = (self._seed * 1664525 + 1013904223) % (2^32)
            local r = (self._seed % 10000) / 10000
            if min and max then return min + r * (max - min) end
            return r
        end,
        NextInteger = function(self, min, max)
            return math.floor(self:NextNumber(min, max + 1))
        end,
        NextUnitVector = function(self)
            local x = self:NextNumber(-1,1)
            local y = self:NextNumber(-1,1)
            local z = self:NextNumber(-1,1)
            local len = math.sqrt(x*x+y*y+z*z)
            if len < 0.001 then return Vector3.new(0,1,0) end
            return Vector3.new(x/len, y/len, z/len)
        end,
    }
    return setmetatable(state, mt)
end

-- ══════════════════════════════════════════════════════════════════════
--  string extras — métodos adicionales compatibles con Roblox
-- ══════════════════════════════════════════════════════════════════════
if not string.split then
    function string.split(s, sep)
        local result, pattern = {}, "([^" .. (sep or ",") .. "]+)"
        for part in string.gmatch(s, pattern) do table.insert(result, part) end
        return result
    end
end

if not string.trim then
    function string.trim(s) return s:match("^%s*(.-)%s*$") end
end

-- ══════════════════════════════════════════════════════════════════════
--  table extras
-- ══════════════════════════════════════════════════════════════════════
table.find  = table.find  or function(t, v) for i, x in ipairs(t) do if x == v then return i end end end
table.clear = table.clear or function(t) for k in next, t do t[k] = nil end end
table.clone = table.clone or function(t) local c = {} for k, v in pairs(t) do c[k] = v end return c end
table.freeze = table.freeze or function(t)
    local mt = getmetatable(t) or {}
    mt.__newindex = function() error("attempt to modify a frozen table", 2) end
    mt.__frozen = true
    setmetatable(t, mt)
    return t
end
table.isfrozen = table.isfrozen or function(t)
    local mt = getmetatable(t)
    return mt ~= nil and mt.__frozen == true
end

-- ══════════════════════════════════════════════════════════════════════
--  math extras
-- ══════════════════════════════════════════════════════════════════════
math.clamp = math.clamp or function(v, mn, mx) return v < mn and mn or v > mx and mx or v end
math.lerp  = math.lerp  or function(a, b, t) return a + (b - a) * t end
math.sign  = math.sign  or function(x) return x > 0 and 1 or x < 0 and -1 or 0 end
math.round = math.round or function(x) return math.floor(x + 0.5) end
math.map   = math.map   or function(v, inMin, inMax, outMin, outMax)
    return outMin + (v - inMin) / (inMax - inMin) * (outMax - outMin)
end
math.noise = math.noise or (function()
    local p={151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,
        21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,88,237,
        149,56,87,174,20,125,136,171,168,68,175,74,165,71,134,139,48,27,166,77,146,158,231,83,111,
        229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,
        209,76,132,187,208,89,18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,
        217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,
        28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,172,9,129,22,39,
        253,19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,228,251,34,242,193,238,210,144,
        12,191,179,162,241,81,51,145,235,249,14,239,107,49,192,214,31,181,199,106,157,184,84,204,
        176,115,121,50,45,127,4,150,254,138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180}
    local function perm(i) return p[(i % 256) + 1] end
    local function fade(t) return t*t*t*(t*(t*6-15)+10) end
    local function lerp2(t,a,b) return a+t*(b-a) end
    local function grad(h,x,y,z)
        h=h%16; local u=h<8 and x or y; local v=h<4 and y or (h==12 or h==14) and x or z
        return ((h%2==0) and u or -u)+((math.floor(h/2)%2==0) and v or -v)
    end
    return function(x,y,z)
        y=y or 0; z=z or 0
        local xi=math.floor(x)%256; local yi=math.floor(y)%256; local zi=math.floor(z)%256
        x=x-math.floor(x); y=y-math.floor(y); z=z-math.floor(z)
        local u=fade(x); local v=fade(y); local w=fade(z)
        local A=perm(xi)+yi; local AA=perm(A)+zi; local AB=perm(A+1)+zi
        local B=perm(xi+1)+yi; local BA=perm(B)+zi; local BB=perm(B+1)+zi
        return lerp2(w,
            lerp2(v,lerp2(u,grad(perm(AA),x,y,z),grad(perm(BA),x-1,y,z)),
                    lerp2(u,grad(perm(AB),x,y-1,z),grad(perm(BB),x-1,y-1,z))),
            lerp2(v,lerp2(u,grad(perm(AA+1),x,y,z-1),grad(perm(BA+1),x-1,y,z-1)),
                    lerp2(u,grad(perm(AB+1),x,y-1,z-1),grad(perm(BB+1),x-1,y-1,z-1))))
    end
end)()

-- ══════════════════════════════════════════════════════════════════════
--  Region3 — 3D region defined by two Vector3 (min/max)
-- ////
--  Region3 — región 3D definida por dos Vector3 (min/max)
-- ══════════════════════════════════════════════════════════════════════
Region3 = {}
Region3.__index = Region3
function Region3.new(minV, maxV)
    minV = minV or Vector3.new(0,0,0)
    maxV = maxV or Vector3.new(0,0,0)
    local t = { Min = minV, Max = maxV }
    t.Size  = Vector3.new(maxV.X-minV.X, maxV.Y-minV.Y, maxV.Z-minV.Z)
    t.CFrame = CFrame.new((minV.X+maxV.X)*0.5, (minV.Y+maxV.Y)*0.5, (minV.Z+maxV.Z)*0.5)
    return setmetatable(t, Region3)
end
function Region3:ExpandToGrid(resolution)
    local function snap(v) return math.floor(v/resolution+0.5)*resolution end
    return Region3.new(
        Vector3.new(snap(self.Min.X),snap(self.Min.Y),snap(self.Min.Z)),
        Vector3.new(snap(self.Max.X),snap(self.Max.Y),snap(self.Max.Z)))
end
function Region3:__tostring()
    return string.format("Region3(%s, %s)", tostring(self.Min), tostring(self.Max))
end

-- ══════════════════════════════════════════════════════════════════════
--  Rect — 2D rectangle with min/max Vector2
-- ////
--  Rect — rectángulo 2D con min/max Vector2
-- ══════════════════════════════════════════════════════════════════════
Rect = {}
Rect.__index = Rect
function Rect.new(a, b, c, d)
    local min, max
    if type(a) == "table" then
        min = a; max = b or Vector2.new(0,0)
    else
        min = Vector2.new(a or 0, b or 0); max = Vector2.new(c or 0, d or 0)
    end
    return setmetatable({ Min=min, Max=max, Width=max.X-min.X, Height=max.Y-min.Y }, Rect)
end
function Rect:__tostring()
    return string.format("Rect(%s, %s)", tostring(self.Min), tostring(self.Max))
end

-- ══════════════════════════════════════════════════════════════════════
--  NumberSequenceKeypoint / NumberSequence
-- ══════════════════════════════════════════════════════════════════════
NumberSequenceKeypoint = {}
NumberSequenceKeypoint.__index = NumberSequenceKeypoint
function NumberSequenceKeypoint.new(t, v, envelope)
    return setmetatable({ Time=t, Value=v, Envelope=envelope or 0 }, NumberSequenceKeypoint)
end

NumberSequence = {}
NumberSequence.__index = NumberSequence
function NumberSequence.new(v)
    if type(v) == "number" then
        return setmetatable({ Keypoints={NumberSequenceKeypoint.new(0,v), NumberSequenceKeypoint.new(1,v)} }, NumberSequence)
    end
    return setmetatable({ Keypoints=v }, NumberSequence)
end

-- ══════════════════════════════════════════════════════════════════════
--  ColorSequenceKeypoint / ColorSequence
-- ══════════════════════════════════════════════════════════════════════
ColorSequenceKeypoint = {}
ColorSequenceKeypoint.__index = ColorSequenceKeypoint
function ColorSequenceKeypoint.new(t, color)
    return setmetatable({ Time=t, Color=color }, ColorSequenceKeypoint)
end

ColorSequence = {}
ColorSequence.__index = ColorSequence
function ColorSequence.new(v)
    if type(v) == "table" and v[1] and v[1].Time ~= nil then
        return setmetatable({ Keypoints=v }, ColorSequence)
    end
    local c = v or Color3.new(1,1,1)
    return setmetatable({ Keypoints={ColorSequenceKeypoint.new(0,c), ColorSequenceKeypoint.new(1,c)} }, ColorSequence)
end

-- ══════════════════════════════════════════════════════════════════════
--  BrickColor — named colors Roblox-style
-- ////
--  BrickColor — colores con nombre estilo Roblox
-- ══════════════════════════════════════════════════════════════════════
BrickColor = {}
BrickColor.__index = BrickColor
local _bc_palette = {
    White=Color3.new(0.94,0.94,0.94), LightGrey=Color3.new(0.78,0.78,0.78),
    Grey=Color3.new(0.63,0.65,0.64),  DarkGrey=Color3.new(0.42,0.42,0.42),
    Black=Color3.new(0.10,0.10,0.10), Red=Color3.new(0.77,0.16,0.11),
    DarkRed=Color3.new(0.47,0.08,0.05), BrightRed=Color3.new(0.77,0.16,0.11),
    Orange=Color3.new(0.86,0.52,0.13), BrightOrange=Color3.new(0.86,0.52,0.13),
    Yellow=Color3.new(0.99,0.86,0.00), BrightYellow=Color3.new(0.99,0.86,0.00),
    Lime=Color3.new(0.62,0.95,0.13), BrightGreen=Color3.new(0.29,0.59,0.29),
    Green=Color3.new(0.29,0.59,0.29), DarkGreen=Color3.new(0.16,0.40,0.16),
    Cyan=Color3.new(0.01,0.80,0.80), BrightCyan=Color3.new(0.01,0.80,0.80),
    Blue=Color3.new(0.16,0.27,0.66), BrightBlue=Color3.new(0.05,0.41,0.67),
    DarkBlue=Color3.new(0.07,0.13,0.47), Magenta=Color3.new(0.64,0.31,0.63),
    Pink=Color3.new(0.91,0.62,0.74), Tan=Color3.new(0.84,0.77,0.62),
    Brown=Color3.new(0.49,0.35,0.24), Sand=Color3.new(0.75,0.72,0.60),
}
function BrickColor.new(v)
    local color
    if type(v) == "string" then
        color = _bc_palette[v] or Color3.new(0.63,0.65,0.64)
        return setmetatable({ Name=v, Color=color, Number=0 }, BrickColor)
    elseif type(v) == "number" then
        return setmetatable({ Name="Color"..v, Color=Color3.new(0.63,0.65,0.64), Number=v }, BrickColor)
    end
    return setmetatable({ Name="Medium stone grey", Color=Color3.new(0.63,0.65,0.64), Number=0 }, BrickColor)
end
function BrickColor.random()
    local names={}; for k in pairs(_bc_palette) do table.insert(names,k) end
    return BrickColor.new(names[math.random(1,#names)])
end
function BrickColor.palette(index)
    local names={}; for k in pairs(_bc_palette) do table.insert(names,k) end
    return BrickColor.new(names[((index or 1)-1)%#names+1])
end
function BrickColor:__tostring() return self.Name end

)LUAU";

#endif // LUAU_STDLIB_H
