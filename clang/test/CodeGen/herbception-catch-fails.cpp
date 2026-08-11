// RUN: %clang -fherbceptions -fno-exceptions -S -emit-llvm -o - %s | FileCheck %s

// Herbception `catch fails(expr)`: the throws call returns {T, i1}, and the
// expression builds an either{T, E} value with .positive = !discriminant and
// .left/.right sourced from the payload slot.

// CHECK: %struct.either = type { i8, i32, i32 }

// A fails{int} function returns {i32, i1} with the 'throws' attribute.
// CHECK: define dso_local { i32, i1 } @_Z3bari(i32 noundef %x) #[[ATTR:[0-9]+]]
int bar(int x) fails{int} {
  if (x < 0) throw throws x;
  return x + 1;
}

// catch fails(bar(x)) extracts the discriminant and builds the either value.
// CHECK-LABEL: define dso_local noundef i32 @_Z3fooi(i32 noundef %x)
// CHECK:         %call = call { i32, i1 } @_Z3bari
// CHECK:         %[[DISC:.*]] = extractvalue { i32, i1 } %call, 1
// CHECK:         %[[POS:.*]] = xor i1 %[[DISC]], true
// CHECK:         %[[POS8:.*]] = zext i1 %[[POS]] to i8
// CHECK:         insertvalue %struct.either poison, i8 %[[POS8]], 0
// CHECK:         insertvalue %struct.either %{{.*}}, i32 %{{.*}}, 1
// CHECK:         insertvalue %struct.either %{{.*}}, i32 %{{.*}}, 2
int foo(int x) {
  auto e = catch fails(bar(x));
  return e.positive ? e.left * 2 : e.right;
}

// CHECK: attributes #[[ATTR]] = { {{.*}}throws{{.*}} }
