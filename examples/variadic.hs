function test(args) no_capture variadic do
	print(args)
end

function test2(a, b, c) no_capture do
	print(a, ", ", b, ", ", c)
end

test2 variadic ([1, 2, 3])

test variadic (for i in irange(10) do i end)