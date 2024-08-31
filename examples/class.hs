class person
    name
    age
end
michael = person("Michael", 19)
michael["name"]

class person2
    name
    age
    year

    function construct(name, age) 
        self.name = name
        self.age = age
        self.year = age - 18
    end

    function say_hi()
        return "Hello there"
    end

    function say_bi()
        return "You are a bold one"
    end
end
michael = person2("Michael", 19)
michael.say_hi()
michael.say_bi()

class student
    name
    age = 18
end
tim = student("Tim")