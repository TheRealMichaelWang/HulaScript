function gcd(a, b) do
	if b == 0 then
		return a
	else
		return gcd(b, a % b)
	end
end

function lcm(a, b) do
	if a > b then
		return (a / gcd(a, b)) * b
	else
		return (b / gcd(a, b)) * a
	end
end