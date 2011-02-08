integer function doubleme(x)
implicit none
! input
integer :: x
! output
integer :: doubleme
      doubleme = x * 2
end

subroutine subby(x,y,z)
implicit none
integer :: x,y,z
      z = x - y
end

program main
implicit none
integer :: x, y, z
integer doubleme

      print*, "Enter a number for x: "
      read*, x
      y = doubleme(x)
      print*, "Enter a number for z: "
      call subby(x,y,z)
      print*, "x = ", x, ", y = ", y, ", z = ", z
end program main
