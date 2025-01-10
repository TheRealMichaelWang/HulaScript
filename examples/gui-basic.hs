gui = fimport("HulaGUI")
app = gui.app("org.gtk.example")
app.connectHandler("activate", function() do
	window = app.newWindow("Hello World", 800, 600)

	box = gui.box("vertical", 0)
	box.setHAlign("center")
	box.setVAlign("center")
	window.setChild(box)
	
	button = gui.button("Hello World")
	button.connectHandler("clicked", function() do
		print("Hello there, General Kenobi!")
		window.close()
	end)
	box.append(button)

	window.present()
end)
app.run()

gui = fimport("HulaGUI")
app = gui.app("org.gtk.example")
app.connectHandler("activate", function() do
	window = app.newWindow("Grid Test", 800, 600)

	grid = gui.grid()
	grid.setHAlign("center")
	grid.setVAlign("center")
	window.setChild(grid)

	button = gui.button("Button 1")
	hello = function() no_capture do
		print("Hello World!")
	end
	button.connectHandler("clicked", hello)
	grid.attach(button, 0, 0, 1, 1)

	button2 = gui.button("Button 2")
	button2.connectHandler("clicked", hello)
	grid.attach(button2, 1, 0, 1, 1)

	button3 = gui.button("Quit")
	button3.connectHandler("clicked", window.close)
	grid.attach(button3, 0, 1, 2, 1)

	window.present()
end)

app.run()