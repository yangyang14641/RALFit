program driver

use nlls_module

integer                   :: n, m, len_work_int, len_work_real, i
real(wp), allocatable     :: X(:), Work_real(:)
integer, allocatable      :: Work_int(:)
type( NLLS_inform_type )  :: status
type( NLLS_control_type ) :: options

write(*,*) '==============='
write(*,*) 'RAL NLLS driver'
write(*,*) '==============='
write(*,*) ' '


n = 4

m = 4

len_work_int = n
allocate( Work_int(len_work_int) )

len_work_real = n
allocate( Work_real(len_work_real) ) 

allocate( X(n) )

X(1) =  3.0 
X(2) = -1.0
X(3) =  0.0
X(4) =  1.0

options%print_level = 3

call ral_nlls(n, m, X, Work_int, len_work_int, & 
              Work_real, len_work_real,         &
              eval_F, eval_J,                   &
              status, options )

do i = 1,n
   write(*,*) X(i)
end do


end program driver


