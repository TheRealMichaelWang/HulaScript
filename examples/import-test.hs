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

function bezout(a, b) do
	if b == 0 then
		return [1, 0, a]
	end

	r = a % b
	q = (a - r) / b

	g = bezout(b, r)
	return [g[1], g[0] - q * g[1], g[2]]
end

function modInv(a, mod) do
	coefs = bezout(a, mod)

	if coefs[2] != 1 then
		return nil
	else
		inv = coefs[0]
		return if inv >= 0 then inv else inv + (inv % mod) end
	end
end