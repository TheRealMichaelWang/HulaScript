function fact(n) no_capture do
	return if n == 0 then 1 else n * fact(n - 1) end
end