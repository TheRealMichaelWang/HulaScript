function fib(n) no_capture do
	return if n <= 1 then n else fib(n - 1) + fib(n - 2) end
end