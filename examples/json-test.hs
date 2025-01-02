class person
	name
	age

	@json_constructor = "person"

	function toJSON() do
		return format("{ \"Name\" : \"%s\", \"Age\" : %d }", self.name, self.age)
	end
end

michael = person("Michael", 19)

utils = fimport("HulaUtils")
src = utils.toJSON([michael, michael, michael])

parser = utils.JSONParser()
parser.addConstructor("person", person, ["name", "age"])

obj = parser.parseJSON(src)

testObj = {
	.name = "Michael",
	.age = 19,

	.@json_keys = ["name", "age"]
}

src = utils.toJSON(testObj)
obj = parser.parseJSON(src)