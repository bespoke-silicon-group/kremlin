program hello
implicit none
integer :: x, y

	print*, "Enter a number for x: "
	read*, x
	y = x * 2
	print*, "x = ", x, ", y = ", y
end program hello
