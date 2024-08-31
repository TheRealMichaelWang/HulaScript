class node
	next = nil
	elem
end

class list
	head = nil
	tail = nil

	function pushBack(elem)
		if self.head == nil then
			self.head = node(elem)
			self.tail = self.head
		else
			self.tail.next = node(elem)
			self.tail = self.tail.next
		end
	end
end