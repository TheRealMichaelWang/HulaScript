#first green thread test
function test(a) no_capture do
	for i in irange(a) do
		print(a, ":", i)
	end
end

for i in irange(10) do
	test async(11-i)
end


#first async test
function test(a) no_capture do
	return a
end
print(await test async(8))


#sleep sort
function sleepSort(toSort) no_capture do
	result = []

	helper = function(i) do
		await sleep(i)
		result.append(i)
	end

	await awaitAll variadic(for element in toSort do helper async(element) end)
	return result
end
sleepSort([3,2,1])


#first lock test
myLock = lock()

function test(a) do
	release = await myLock.lock()
	for i in irange(a) do
		print(a, ":", i)
	end
	release.unlock()
end

for i in irange(10) do
	test start(11-i)
end


#async print/input tests

function test(i) no_capture do
	msg = await inputAsync(i, ": ")
	await printAsync(msg)
end

for i in irange(10) do
	test async(i)
end


# ffi invoke test
function test() no_capture do
	i = 0
	while true do
		i = i + 1
	end
end

function test2() no_capture do
	test async()

	a = [1,2,3,4,5,6,7,8,9,10]
	print(a.filter(function(x) no_capture do return x % 2 == 0 end))
end

test2()