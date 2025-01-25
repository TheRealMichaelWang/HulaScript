function test(a) do
	for i in irange(a) do
		print(a, ":", i)
	end
end

for i in irange(10) do
	test start(i)
end