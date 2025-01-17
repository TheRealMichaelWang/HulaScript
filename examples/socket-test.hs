#Listener
utils = fimport("HulaUtils")

listener = utils.listenerSocket(25)
listener.listen(8)

client = listener.accept()
client.recvAscii()


#Client
utils = fimport("HulaUtils")

client = utils.socketConnect("127.0.0.1", 25)
client.sendAscii("Hello!")


#Fancy Server
utils = fimport("HulaUtils")

listener = utils.listenerSocket("25")
listener.listen(3)

clients = []
while True do
	if listener.hasBacklog() then
		print("New Connection...")
		client = listener.accept()
		client.setTimeout(5000)
		client.sendAscii("You are connected!")
		clients.append(client)
	
	for client in clients do
		if client.availible() then
			try
				command = client.recvAscii()
				client.sendAscii(command)
			except:
				
			end
		end
	end
end


#Fancy Client
utils = fimport("HulaUtils")

client = utils.socketConnect("localhost", "25")

while True do
	print(client.recvAscii())

	command = input(">>> ")
	client.sendAscii()

	if command == "quit" then
		break
	end
end