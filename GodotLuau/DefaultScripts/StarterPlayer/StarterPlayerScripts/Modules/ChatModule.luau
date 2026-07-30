-- > GodotLuau — PimpoliDev
-- ChatModule.lua — ModuleScript — Modules/
-- Lógica del chat, en Luau. Guarda los mensajes y habla con TextChatService.
-- Edítalo a tu gusto.
-- /// Chat logic, in Luau. Keeps the messages and talks to TextChatService.
-- /// Edit it however you like.

local ChatModule = {}

local TextChatService = game:GetService("TextChatService")

-- CONFIG del chat. /// Chat CONFIG.
ChatModule.MaxMessages  = 50    -- máximo de mensajes guardados /// max messages kept
ChatModule.FadeDelay    = 8.0   -- segundos hasta desvanecer /// seconds until fade out
ChatModule.FilterWords  = true  -- filtrar palabras inapropiadas /// filter bad words

local messages  = {}
local chat_open = false
local initialized = false

-- Prepara el chat y escucha los mensajes que llegan. /// Sets up the chat and listens for incoming messages.
function ChatModule:Initialize()
    if initialized then return end
    initialized = true

    if TextChatService and TextChatService.MessageReceived then
        TextChatService.MessageReceived:Connect(function(msg)
            self:_addMessage(msg)
        end)
    end

    dprint("[ChatModule] Chat listo. / o Enter para abrir.")
end

function ChatModule:_addMessage(text)
    if type(text) ~= "string" then return end
    if #messages >= self.MaxMessages then
        table.remove(messages, 1)
    end
    table.insert(messages, { text = text, time = 0.0 })
    dprint("[Chat] " .. text)
end

function ChatModule:SendMessage(text)
    if not text or text == "" then return end
    if TextChatService then
        TextChatService:SendMessage(text)
    end
    self:_addMessage(text)
end

function ChatModule:IsOpen()
    return chat_open
end

function ChatModule:Open()
    chat_open = true
end

function ChatModule:Close()
    chat_open = false
end

function ChatModule:GetMessages()
    return messages
end

return ChatModule
