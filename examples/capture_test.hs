a = 3

test1 = function() do
	return a
end

person = []
function test2() do
	return function() do
		person.name = "Michael"
		person.age = 18
	end
end
a = test2()
a()

function test3() no_capture do
	mod = import("import-test.hs")
	mod = nil
	@garbage_collect
end
test3()