! RUN: bbc -emit-hlfir %s -o - | FileCheck %s --check-prefixes=CHECK%if target=x86_64{{.*}} %{,CHECK-KIND10%}%if flang-supports-f128-math %{,CHECK-KIND16%}

integer(1) function fct1()
end
! CHECK-LABEL: func @_QPfct1() -> i8
! CHECK:         return %{{.*}} : i8

integer(2) function fct2()
end
! CHECK-LABEL: func @_QPfct2() -> i16
! CHECK:         return %{{.*}} : i16

integer(4) function fct3()
end
! CHECK-LABEL: func @_QPfct3() -> i32
! CHECK:         return %{{.*}} : i32

integer(8) function fct4()
end
! CHECK-LABEL: func @_QPfct4() -> i64
! CHECK:         return %{{.*}} : i64

integer(16) function fct5()
end
! CHECK-LABEL: func @_QPfct5() -> i128
! CHECK:         return %{{.*}} : i128

function fct()
  integer :: fct
end
! CHECK-LABEL: func @_QPfct() -> i32
! CHECK:         return %{{.*}} : i32

function fct_res() result(res)
  integer :: res
end
! CHECK-LABEL: func @_QPfct_res() -> i32
! CHECK:         return %{{.*}} : i32

integer function fct_body()
  goto 1
  1 stop
end

! CHECK-LABEL: func @_QPfct_body() -> i32
! CHECK:         cf.br ^bb1
! CHECK:       ^bb1
! CHECK:         fir.call @_FortranAStopStatement
! CHECK:         fir.unreachable

function fct_iarr1()
  integer, dimension(10) :: fct_iarr1
end

! CHECK-LABEL: func @_QPfct_iarr1() -> !fir.array<10xi32>
! CHECK:         return %{{.*}} : !fir.array<10xi32>

function fct_iarr2()
  integer, dimension(10, 20) :: fct_iarr2
end

! CHECK-LABEL: func @_QPfct_iarr2() -> !fir.array<10x20xi32>
! CHECK:         return %{{.*}} : !fir.array<10x20xi32>

logical(1) function lfct1()
end
! CHECK-LABEL: func @_QPlfct1() -> !fir.logical<1>
! CHECK:         return %{{.*}} : !fir.logical<1>

logical(2) function lfct2()
end
! CHECK-LABEL: func @_QPlfct2() -> !fir.logical<2>
! CHECK:         return %{{.*}} : !fir.logical<2>

logical(4) function lfct3()
end
! CHECK-LABEL: func @_QPlfct3() -> !fir.logical<4>
! CHECK:         return %{{.*}} : !fir.logical<4>

logical(8) function lfct4()
end
! CHECK-LABEL: func @_QPlfct4() -> !fir.logical<8>
! CHECK:         return %{{.*}} : !fir.logical<8>

real(2) function rfct1()
end
! CHECK-LABEL: func @_QPrfct1() -> f16
! CHECK:         return %{{.*}} : f16

real(3) function rfct2()
end
! CHECK-LABEL: func @_QPrfct2() -> bf16
! CHECK:         return %{{.*}} : bf16

real function rfct3()
end
! CHECK-LABEL: func @_QPrfct3() -> f32
! CHECK:         return %{{.*}} : f32

real(8) function rfct4()
end
! CHECK-LABEL: func @_QPrfct4() -> f64
! CHECK:         return %{{.*}} : f64

function rfct5()
  integer, parameter :: kind10 = merge(10, 4, selected_real_kind(p=18).eq.10)
  real(kind10) :: rfct5
end
! CHECK-KIND10-LABEL: func @_QPrfct5() -> f80
! CHECK-KIND10:         return %{{.*}} : f80

function rfct6()
  integer, parameter :: kind16 = merge(16, 4, selected_real_kind(p=33).eq.16)
  real(kind16) :: rfct6
end
! CHECK-KIND16-LABEL: func @_QPrfct6() -> f128
! CHECK-KIND16:         return %{{.*}} : f128

complex(2) function cplxfct1()
end
! CHECK-LABEL: func @_QPcplxfct1() -> complex<f16>
! CHECK:         return %{{.*}} : complex<f16>

complex(3) function cplxfct2()
end
! CHECK-LABEL: func @_QPcplxfct2() -> complex<bf16>
! CHECK:         return %{{.*}} : complex<bf16>

complex(4) function cplxfct3()
end
! CHECK-LABEL: func @_QPcplxfct3() -> complex<f32>
! CHECK:         return %{{.*}} : complex<f32>

complex(8) function cplxfct4()
end
! CHECK-LABEL: func @_QPcplxfct4() -> complex<f64>
! CHECK:         return %{{.*}} : complex<f64>

function cplxfct5()
  integer, parameter :: kind10 = merge(10, 4, selected_real_kind(p=18).eq.10)
  complex(kind10) :: cplxfct5
end
! CHECK-KIND10-LABEL: func @_QPcplxfct5() -> complex<f80>
! CHECK-KIND10:         return %{{.*}} : complex<f80>

function cplxfct6()
  integer, parameter :: kind16 = merge(16, 4, selected_real_kind(p=33).eq.16)
  complex(kind16) :: cplxfct6
end
! CHECK-KIND16-LABEL: func @_QPcplxfct6() -> complex<f128>
! CHECK-KIND16:         return %{{.*}} : complex<f128>

function fct_with_character_return(i)
  character(10) :: fct_with_character_return
  integer :: i
end
! CHECK-LABEL: func @_QPfct_with_character_return(
! CHECK-SAME: %{{.*}}: !fir.ref<!fir.char<1,10>>{{.*}}, %{{.*}}: index{{.*}}, %{{.*}}: !fir.ref<i32>{{.*}}) -> !fir.boxchar<1> {
