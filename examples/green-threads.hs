function test(a) do
	for i in irange(a) do
		print(a, ":", i)
	end
end

for i in irange(10) do
	test start(11-i)
end

function test(a) no_capture do
	return a
end
print(await test start(8))