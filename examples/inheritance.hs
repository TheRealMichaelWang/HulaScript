class animal
	name
	species

	function sayHi()
		return "Hi!"
	end
end

class dog(animal)
	wagging

	function construct(name) : (name, "dog")
		wagging = name == "Fido"
	end

	function sayHi()
		return "Woof! Woof!"
	end
end
fido = dog("Fido")
fido.wagging