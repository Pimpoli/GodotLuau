#ifndef LUAU_STDLIB_H
#define LUAU_STDLIB_H

// ════════════════════════════════════════════════════════════════════
//  GodotLuau stdlib — servicios implementados en Lua puro.
//  Se ejecuta al inicio de cada ScriptNodeBase antes del script del usuario.
//  Los servicios se guardan en _G["__SVC_NombreServicio"] para que
//  game:GetService() los encuentre sin necesidad de un nodo C++.
// ════════════════════════════════════════════════════════════════════

static const char* LUAU_STDLIB_CODE = R"LUAU(

-- ══════════════════════════════════════════════════════════════════════
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
--  Enum — constantes de enumeración exactas de Roblox
-- ══════════════════════════════════════════════════════════════════════
Enum = {
    EasingStyle = {
        Linear = 0, Sine = 1, Back = 2, Bounce = 3,
        Elastic = 4, Exponential = 5, Circular = 6,
        Quad = 7, Quart = 8, Quint = 9, Cubic = 10,
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
}

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
--  TweenService — interpolación de propiedades estilo Roblox
-- ══════════════════════════════════════════════════════════════════════
local function _ease(styleEnum, dirEnum, t)
    if t <= 0 then return 0 elseif t >= 1 then return 1 end
    local s = styleEnum or 7  -- Quad por defecto

    local function easeIn(x)
        if s == 0 then return x                                                    -- Linear
        elseif s == 7 then return x * x                                            -- Quad
        elseif s == 10 then return x * x * x                                       -- Cubic
        elseif s == 8 then return x * x * x * x                                    -- Quart
        elseif s == 9 then return x * x * x * x * x                               -- Quint
        elseif s == 1 then return 1 - math.cos(x * math.pi * 0.5)                 -- Sine
        elseif s == 5 then return x == 0 and 0 or math.pow(2, 10 * x - 10)        -- Exponential
        elseif s == 6 then return 1 - math.sqrt(1 - x * x)                        -- Circular
        elseif s == 2 then -- Back
            local c1 = 1.70158
            return (c1 + 1) * x * x * x - c1 * x * x
        elseif s == 3 then -- Bounce In = 1 - bounceOut(1-x)
            local bx = 1 - x
            local n1, d1 = 7.5625, 2.75
            local r
            if bx < 1 / d1 then r = n1 * bx * bx
            elseif bx < 2 / d1 then bx -= 1.5 / d1; r = n1 * bx * bx + 0.75
            elseif bx < 2.5 / d1 then bx -= 2.25 / d1; r = n1 * bx * bx + 0.9375
            else bx -= 2.625 / d1; r = n1 * bx * bx + 0.984375 end
            return 1 - r
        elseif s == 4 then -- Elastic
            local c4 = (2 * math.pi) / 3
            return x == 0 and 0 or -math.pow(2, 10 * x - 10) * math.sin((x * 10 - 10.75) * c4)
        end
        return x
    end

    local function easeOut(x)
        if s == 0 then return x
        elseif s == 7 then local v = 1 - x; return 1 - v * v
        elseif s == 10 then local v = 1 - x; return 1 - v * v * v
        elseif s == 8 then local v = 1 - x; return 1 - v * v * v * v
        elseif s == 9 then local v = 1 - x; return 1 - v * v * v * v * v
        elseif s == 1 then return math.sin(x * math.pi * 0.5)
        elseif s == 5 then return x == 1 and 1 or 1 - math.pow(2, -10 * x)
        elseif s == 6 then local v = x - 1; return math.sqrt(1 - v * v)
        elseif s == 2 then -- Back
            local c1 = 1.70158; local c3 = c1 + 1; local v = x - 1
            return 1 + c3 * v * v * v + c1 * v * v
        elseif s == 3 then -- Bounce Out
            local n1, d1 = 7.5625, 2.75
            if x < 1 / d1 then return n1 * x * x
            elseif x < 2 / d1 then x -= 1.5 / d1; return n1 * x * x + 0.75
            elseif x < 2.5 / d1 then x -= 2.25 / d1; return n1 * x * x + 0.9375
            else x -= 2.625 / d1; return n1 * x * x + 0.984375 end
        elseif s == 4 then -- Elastic
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
    -- Color3: tables con r,g,b
    if ta == "table" and tb == "table" then
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
    }

    function tween:Play()
        if self._playing then return end
        self._playing   = true
        self._cancelled = false
        self.PlaybackState = "Playing"
        local info = tweenInfo
        local gls  = goals

        task.spawn(function()
            local dur     = info.Time or 1
            local style   = info.EasingStyle or Enum.EasingStyle.Quad
            local dir     = info.EasingDirection or Enum.EasingDirection.Out
            local delay   = info.DelayTime or 0
            local repeats = info.RepeatCount or 0
            local rev     = info.Reverses or false

            if delay > 0 then wait(delay) end
            if self._cancelled then self._playing = false; return end

            -- Capturar valores iniciales con pcall
            local startVals = {}
            for prop, _ in pairs(gls) do
                local ok, v = pcall(function() return instance[prop] end)
                if ok then startVals[prop] = v end
            end

            local rep = 0
            repeat
                local startT  = time()
                local forward = true

                while self._playing and not self._cancelled do
                    local elapsed = time() - startT
                    if elapsed > dur then elapsed = dur end
                    local t = dur > 0 and elapsed / dur or 1
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
                            startT  = time()
                        else
                            break
                        end
                    end
                    task.wait(0)
                end

                rep += 1
            until self._cancelled or rep > repeats

            self._playing = false
            if not self._cancelled then
                self.PlaybackState = "Completed"
                for _, fn in ipairs(completedCbs) do pcall(fn) end
            end
        end)
    end

    function tween:Pause()
        self._playing = false
        self.PlaybackState = "Paused"
    end

    function tween:Cancel()
        self._cancelled = true
        self._playing   = false
        self.PlaybackState = "Cancelled"
    end

    return tween
end

_G["__SVC_TweenService"] = TweenService
)LUAU"
R"LUAU(

-- ══════════════════════════════════════════════════════════════════════
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

function CollectionService:AddTag(instance, tag)
    pcall(function() instance:SetAttribute("__TAG_" .. tag, true) end)
    if _ccs_listeners["InstanceTagged:" .. tag] then
        for _, fn in ipairs(_ccs_listeners["InstanceTagged:" .. tag]) do pcall(fn, instance) end
    end
end

function CollectionService:RemoveTag(instance, tag)
    pcall(function() instance:SetAttribute("__TAG_" .. tag, nil) end)
    if _ccs_listeners["InstanceUntagged:" .. tag] then
        for _, fn in ipairs(_ccs_listeners["InstanceUntagged:" .. tag]) do pcall(fn, instance) end
    end
end

function CollectionService:HasTag(instance, tag)
    local ok, v = pcall(function() return instance:GetAttribute("__TAG_" .. tag) end)
    return ok and v == true
end

function CollectionService:GetTagged(tag)
    return {}
end

function CollectionService:GetTags(instance)
    return {}
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
--  ScriptContext — contexto de ejecución de scripts
-- ══════════════════════════════════════════════════════════════════════
local ScriptContext = {}
ScriptContext.__index = ScriptContext

ScriptContext.Error = {
    Connect = function(_, fn)
        -- Podría hookearse al error handler global en el futuro
    end
}

function ScriptContext:GetCurrent() return nil end
function ScriptContext:AddCoreScriptLocal(name, parent) end

_G["__SVC_ScriptContext"] = ScriptContext

-- ══════════════════════════════════════════════════════════════════════
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
    -- UI básica
    ScreenGui="Node", Frame="Node", TextLabel="Node", TextButton="Node",
    TextBox="Node", ImageLabel="Node", ImageButton="Node",
    ScrollingFrame="Node", LocalizationTable="Node",
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

)LUAU";

#endif // LUAU_STDLIB_H
