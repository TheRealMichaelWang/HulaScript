utils = fimport("HulaUtils")

str = utils.toJSON({
	.name = "Michael",
	.age = 19,

	.@json_keys = ["name", "age"]
})

parser = utils.JSONParser()
parser.parseJSON(str)
person = parser.parseJSON(str)

utils.toJSON({
	.name = "Michael",
	.age = 19,

	.@json_keys = ["name", "age"]
})