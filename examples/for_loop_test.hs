class counterIterator
	i
	stop
	step

	function hasNext() do
		return self.i != self.stop
	end

	function next() do
		current = self.i
		self.i = self.i + self.step
		return current
	end
end

class range
	start
	stop
	step

	function iterator() do
		return counterIterator(self.start, self.stop, self.step)
	end
end

a = []
for i in range(0, 10, 1) do
	if i == 5 then
		break
	end
	a[i] = i
else
	a.broken = true
end

k = for i in range(0, 10, 1) do i end

b = []
for i in range(0, 10, 1) do
	if i == 5 then
		continue
	end
	b[i] = i
end

class arrayIterator
	arr
	i = 0

	function hasNext() do
		return self.i != self.arr.@length
	end

	function next() do
		toret = self.arr[self.i]
		self.i = self.i + 1
		return toret
	end
end

a = [1,2,3,5,7,8]
a.iterator = function() do
	return arrayIterator(a)
end

for i in irange(10) do
	print(i)
end