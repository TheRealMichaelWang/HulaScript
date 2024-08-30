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
end
michael = person2("Michael", 19)
michael.year