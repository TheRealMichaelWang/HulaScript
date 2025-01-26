#first green thread test
function test(a) no_capture do
	for i in irange(a) do
		print(a, ":", i)
	end
end

for i in irange(10) do
	test start(11-i)
end


#first async test
function test(a) no_capture do
	return a
end
print(await test start(8))


#sleep sort
function sleepSort(toSort) no_capture do
	result = []

	helper = function(i) do
		await sleep(i)
		result.append(i)
	end

	await awaitAll variadic(for element in toSort do helper start(element) end)
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

#crashes for some reason
myLock = lock()

function test(a) no_capture do
	release = lock.lock()
	for i in irange(a) do
		print(a, ":", i)
	end
	release.unlock()
end

for i in irange(10) do
	test start(11-i)
end
lock.lock()