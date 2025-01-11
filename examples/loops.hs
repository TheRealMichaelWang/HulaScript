i = 0
while i < 10 do
	i = i + 1
end

i = 0
while i < 10 do
	if i == 5 then
		break
	end
	i = i + 1
end

i = 0
do
	i = i + 1
while i != 10

i = 0
do
	i = i + 1
	if i == 5 then
		break
	end
while i != 10

function r(n) no_capture do
    if n <= 0 then
            return nil
    else
            print(n)
            r(n - 2)
            print(n - 1)
    end
end