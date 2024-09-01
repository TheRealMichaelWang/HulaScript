class counterIterator
	i
	stop
	step

	function hasNext()
		return self.i != self.stop
	end

	function next()
		current = self.i
		self.i = self.i + self.step
		return current
	end
end

class range
	start
	stop
	step

	function iterator()
		return counterIterator(self.start, self.stop, self.step)
	end
end

a = []
for i in range(0, 10, 1) do
	a[i] = i
end