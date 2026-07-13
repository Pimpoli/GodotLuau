local TCS = game:GetService("TextChatService")
print("CHAT-start")
if TCS and TCS.MessageReceived then
	print("CHAT-member-ok")
	TCS.MessageReceived:Connect(function(msg)
		print("CHAT-RECV", msg)
	end)
end
task.wait(0.5)
TCS:SendMessage("hola desde un solo argumento")
TCS:SendMessage("Pimpoli", "hola con nombre")
print("CHAT-sent")
