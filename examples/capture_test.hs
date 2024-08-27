a = 3

test1 = function()
	return a
end

person = []
function test2()
	return function()
		person.name = "Michael"
		person.age = 18
	end
end