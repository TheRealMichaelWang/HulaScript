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

class student(function(name, age)
    person = [1,2,3]
    person.name = name
    person.age = age
	return person
end)
    grade

    function construct(name, age, grade) : (name, age)
        self.grade = grade
	end
end

tim = student("Tim", 9000, 1)