gui = fimport("HulaGUI")
app = gui.app("org.gtk.example")

app.connectHandler("activate", function() do
	window = app.newWindow("Calculator", 600, 400)

	grid = gui.grid()
	grid.setDock("fill")
	window.setChild(grid)

	inputBox = gui.input()
	inputBox.setHAlign("fill")
	inputBox.setVAlign("center")
	inputBox.setHExpand(true)
	grid.attach(inputBox, 0, 0, 4, 1)

	count = 1
	for i in irange(3) do
		for j in irange(3) do
			button = gui.button(format("%d", count))
			button.connectHandler("clicked", function() do
				inputBox.setText(format("%s%d", inputBox.getText(), count))
			end)
			grid.attach(button, i, j + 1, 1, 1)
			
			count = count + 1
		end
	end

	operators = ["+", "-", "*", "/"]
	i = 1
	for operator in operators do
		button = gui.button(operator)
		button.connectHandler("clicked", function() do
			inputBox.setText(format("%s%s", inputBox.getText(), operator))
		end)

		grid.attach(button, 3, i, 1, 1)
		i = i + 1
	end

	window.present()
end)
app.run()