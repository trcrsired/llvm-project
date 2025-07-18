# RUN: %PYTHON %s 2>&1 | FileCheck %s

import ctypes
import sys
from mlir.ir import *
from mlir.dialects import builtin
from mlir.dialects import func
from mlir.dialects import linalg
from mlir.passmanager import *
from mlir.execution_engine import *

from mlir.dialects.linalg.opdsl.lang import *


# Log everything to stderr and flush so that we have a unified stream to match
# errors/info emitted by MLIR to stderr.
def log(*args):
    print(*args, file=sys.stderr)
    sys.stderr.flush()


fill_boiler = """
func.func @main() -> i32 attributes {llvm.emit_c_interface} {
  %O0 = memref.alloc() : memref<i32>
  %O1 = memref.alloc() : memref<16xi32>
  %O2 = memref.alloc() : memref<4x16xi32>

  %val0 = arith.constant 1.0 : f32
  %val1 = arith.constant 2.0 : f32
  %val2 = arith.constant 3.0 : f32

  call @fill_0d_on_buffers(%val0, %O0) : (f32, memref<i32>) -> ()
  call @fill_1d_on_buffers(%val1, %O1) : (f32, memref<16xi32>) -> ()
  call @fill_2d_on_buffers(%val2, %O2) : (f32, memref<4x16xi32>) -> ()

  %c0 = arith.constant 0 : index
  %res0 = memref.load %O0[] : memref<i32>
  %c8 = arith.constant 8 : index
  %res1 = memref.load %O1[%c8] : memref<16xi32>
  %c2 = arith.constant 2 : index
  %res2 = memref.load %O2[%c2, %c8] : memref<4x16xi32>

  %0 = arith.addi %res0, %res1 : i32
  %1 = arith.addi %0, %res2 : i32

  // TODO: FFI-based solution to allow testing and printing with python code.
  return %1 : i32
}
"""

fill_rng_boiler = """
func.func @main() -> i32 attributes {llvm.emit_c_interface} {
  %O = memref.alloc() : memref<4x16xi32>
  %min = arith.constant -1000.0 : f64
  %max = arith.constant 1000.0 : f64
  %seed = arith.constant 42 : i32

  call @fill_rng_on_buffers(%min, %max, %seed, %O) :
    (f64, f64, i32, memref<4x16xi32>) -> ()

  %c0 = arith.constant 0 : index
  %0 = memref.load %O[%c0, %c0] : memref<4x16xi32>

  // TODO: FFI-based solution to allow testing and printing with python code.
  return %0 : i32
}
"""

conv_boiler = """
func.func @main() -> i32 attributes {llvm.emit_c_interface} {
  %v0 = arith.constant 0 : i32
  %v1 = arith.constant 1.0 : f64
  %v2 = arith.constant 2.0 : f64

  %input = memref.alloc() : memref<1x4x16x1xf64>
  %filter = memref.alloc() : memref<2x2x1xf64>
  %output = memref.alloc() : memref<1x2x4x1xi32>
  linalg.fill ins(%v1 : f64) outs(%input : memref<1x4x16x1xf64>)
  linalg.fill ins(%v2 : f64) outs(%filter : memref<2x2x1xf64>)
  linalg.fill ins(%v0 : i32) outs(%output : memref<1x2x4x1xi32>)

  call @conv_on_buffers(%input, %filter, %output) :
    (memref<1x4x16x1xf64>, memref<2x2x1xf64>, memref<1x2x4x1xi32>) -> ()

  %c0 = arith.constant 0 : index
  %0 = memref.load %output[%c0, %c0, %c0, %c0] : memref<1x2x4x1xi32>

  // TODO: FFI-based solution to allow testing and printing with python code.
  return %0 : i32
}
"""

pooling_boiler = """
func.func @main() -> i32 attributes {llvm.emit_c_interface} {
  %v0 = arith.constant 0 : i32
  %v42 = arith.constant 42.0 : f64
  %v77 = arith.constant 77.0 : f64
  %v-13 = arith.constant -13.0 : f64
  %v1 = arith.constant 1.0 : f64

  %input = memref.alloc() : memref<1x4x16x1xf64>
  %shape = memref.alloc() : memref<2x2xf64>
  %output = memref.alloc() : memref<1x2x4x1xi32>
  linalg.fill ins(%v1 : f64) outs(%input : memref<1x4x16x1xf64>)
  linalg.fill ins(%v1 : f64) outs(%shape : memref<2x2xf64>)
  linalg.fill ins(%v0 : i32) outs(%output : memref<1x2x4x1xi32>)

  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c2 = arith.constant 2 : index
  memref.store %v42, %input[%c0, %c0, %c0, %c0] : memref<1x4x16x1xf64>
  memref.store %v77, %input[%c0, %c0, %c1, %c0] : memref<1x4x16x1xf64>
  memref.store %v-13, %input[%c0, %c1, %c0, %c0] : memref<1x4x16x1xf64>

  call @pooling_on_buffers(%input, %shape, %output) :
    (memref<1x4x16x1xf64>, memref<2x2xf64>, memref<1x2x4x1xi32>) -> ()

  %0 = memref.load %output[%c0, %c0, %c0, %c0] : memref<1x2x4x1xi32>

  // TODO: FFI-based solution to allow testing and printing with python code.
  return %0 : i32
}
"""


def transform(module, boilerplate):
    # TODO: Allow cloning functions from one module to another.
    # Atm we have to resort to string concatenation.
    ops = module.operation.regions[0].blocks[0].operations
    mod = Module.parse("\n".join([str(op) for op in ops]) + boilerplate)

    pm = PassManager("builtin.module")
    pm.add("func.func(convert-linalg-to-loops)")
    pm.add("func.func(lower-affine)")
    pm.add("func.func(convert-math-to-llvm)")
    pm.add("func.func(convert-scf-to-cf)")
    pm.add("func.func(arith-expand)")
    pm.add("func.func(memref-expand)")
    pm.add("convert-vector-to-llvm")
    pm.add("finalize-memref-to-llvm")
    pm.add("convert-func-to-llvm")
    pm.add("convert-arith-to-llvm")
    pm.add("convert-cf-to-llvm")
    pm.add("reconcile-unrealized-casts")
    pm.run(mod.operation)
    return mod


def test_fill_builtin():
    with Context() as ctx, Location.unknown():
        module = Module.create()
        f32 = F32Type.get()
        i32 = IntegerType.get_signless(32)
        with InsertionPoint(module.body):

            @func.FuncOp.from_py_func(f32, MemRefType.get([], i32))
            def fill_0d_on_buffers(value, out):
                linalg.fill(value, outs=[out])

            @func.FuncOp.from_py_func(f32, MemRefType.get([16], i32))
            def fill_1d_on_buffers(value, out):
                linalg.fill(value, outs=[out])

            @func.FuncOp.from_py_func(f32, MemRefType.get([4, 16], i32))
            def fill_2d_on_buffers(value, out):
                linalg.fill(value, outs=[out])

        execution_engine = ExecutionEngine(transform(module, fill_boiler))

        # TODO: FFI-based solution to allow testing and printing with python code.
        # Prepare arguments: one result i32.
        # Arguments must be passed as pointers.
        c_int_p = ctypes.c_int * 1
        res = c_int_p(-1)
        execution_engine.invoke("main", res)

        log("RESULT: ", res[0])
        # CHECK: RESULT: 6


test_fill_builtin()


def test_fill_generic():
    with Context() as ctx, Location.unknown():
        module = Module.create()
        f32 = F32Type.get()
        i32 = IntegerType.get_signless(32)
        with InsertionPoint(module.body):

            @func.FuncOp.from_py_func(f32, MemRefType.get([], i32))
            def fill_0d_on_buffers(value, out):
                linalg.fill(value, outs=[out], emit_generic=True)

            @func.FuncOp.from_py_func(f32, MemRefType.get([16], i32))
            def fill_1d_on_buffers(value, out):
                linalg.fill(value, outs=[out], emit_generic=True)

            @func.FuncOp.from_py_func(f32, MemRefType.get([4, 16], i32))
            def fill_2d_on_buffers(value, out):
                linalg.fill(value, outs=[out], emit_generic=True)

        execution_engine = ExecutionEngine(transform(module, fill_boiler))

        # TODO: FFI-based solution to allow testing and printing with python code.
        # Prepare arguments: one result i32.
        # Arguments must be passed as pointers.
        c_int_p = ctypes.c_int * 1
        res = c_int_p(-1)
        execution_engine.invoke("main", res)

        log("RESULT: ", res[0])
        # CHECK: RESULT: 6


test_fill_generic()


def test_fill_rng_builtin():
    with Context() as ctx, Location.unknown():
        module = Module.create()
        f64 = F64Type.get()
        i32 = IntegerType.get_signless(32)
        with InsertionPoint(module.body):

            @func.FuncOp.from_py_func(f64, f64, i32, MemRefType.get((4, 16), i32))
            def fill_rng_on_buffers(min, max, seed, out):
                linalg.fill_rng_2d(min, max, seed, outs=[out])

        execution_engine = ExecutionEngine(transform(module, fill_rng_boiler))

        # TODO: FFI-based solution to allow testing and printing with python code.
        # Prepare arguments: one result i32.
        # Arguments must be passed as pointers.
        c_int_p = ctypes.c_int * 1
        res = c_int_p(-1)
        execution_engine.invoke("main", res)

        log("RESULT: ", res[0])
        # CHECK: RESULT: -480


test_fill_rng_builtin()


def test_fill_rng_generic():
    with Context() as ctx, Location.unknown():
        module = Module.create()
        f64 = F64Type.get()
        i32 = IntegerType.get_signless(32)
        with InsertionPoint(module.body):

            @func.FuncOp.from_py_func(f64, f64, i32, MemRefType.get((4, 16), i32))
            def fill_rng_on_buffers(min, max, seed, out):
                linalg.fill_rng_2d(min, max, seed, outs=[out], emit_generic=True)

        execution_engine = ExecutionEngine(transform(module, fill_rng_boiler))

        # TODO: FFI-based solution to allow testing and printing with python code.
        # Prepare arguments: one result i32.
        # Arguments must be passed as pointers.
        c_int_p = ctypes.c_int * 1
        res = c_int_p(-1)
        execution_engine.invoke("main", res)

        log("RESULT: ", res[0])
        # CHECK: RESULT: -480


test_fill_rng_generic()


def test_max_pooling_builtin():
    with Context() as ctx, Location.unknown():
        module = Module.create()
        f64 = F64Type.get()
        i32 = IntegerType.get_signless(32)
        with InsertionPoint(module.body):

            @func.FuncOp.from_py_func(
                MemRefType.get((1, 4, 16, 1), f64),
                MemRefType.get((2, 2), f64),
                MemRefType.get((1, 2, 4, 1), i32),
            )
            def pooling_on_buffers(input, shape, output):
                linalg.pooling_nhwc_max(
                    input, shape, outs=[output], strides=[2, 4], dilations=[1, 2]
                )

        execution_engine = ExecutionEngine(transform(module, pooling_boiler))

        # TODO: FFI-based solution to allow testing and printing with python code.
        # Prepare arguments: one result i32.
        # Arguments must be passed as pointers.
        c_int_p = ctypes.c_int * 1
        res = c_int_p(-1)
        execution_engine.invoke("main", res)

        log("RESULT: ", res[0])
        # 77 is not selected due to the dilation 2 in the second dimension.
        # CHECK: RESULT: 42


test_max_pooling_builtin()


def test_max_pooling_generic():
    with Context() as ctx, Location.unknown():
        module = Module.create()
        f64 = F64Type.get()
        i32 = IntegerType.get_signless(32)
        with InsertionPoint(module.body):

            @func.FuncOp.from_py_func(
                MemRefType.get((1, 4, 16, 1), f64),
                MemRefType.get((2, 2), f64),
                MemRefType.get((1, 2, 4, 1), i32),
            )
            def pooling_on_buffers(input, shape, output):
                linalg.pooling_nhwc_max(
                    input,
                    shape,
                    outs=[output],
                    strides=[2, 4],
                    dilations=[1, 2],
                    emit_generic=True,
                )

        execution_engine = ExecutionEngine(transform(module, pooling_boiler))

        # TODO: FFI-based solution to allow testing and printing with python code.
        # Prepare arguments: one result i32.
        # Arguments must be passed as pointers.
        c_int_p = ctypes.c_int * 1
        res = c_int_p(-1)
        execution_engine.invoke("main", res)

        log("RESULT: ", res[0])
        # 77 is not selected due to the dilation 2 in the second dimension.
        # CHECK: RESULT: 42


test_max_pooling_generic()


def test_min_pooling_builtin():
    with Context() as ctx, Location.unknown():
        module = Module.create()
        f64 = F64Type.get()
        i32 = IntegerType.get_signless(32)
        with InsertionPoint(module.body):

            @func.FuncOp.from_py_func(
                MemRefType.get((1, 4, 16, 1), f64),
                MemRefType.get((2, 2), f64),
                MemRefType.get((1, 2, 4, 1), i32),
            )
            # Set the strides and use the default dilations.
            def pooling_on_buffers(input, shape, output):
                linalg.pooling_nhwc_min(input, shape, outs=[output], strides=[2, 4])

        execution_engine = ExecutionEngine(transform(module, pooling_boiler))

        # TODO: FFI-based solution to allow testing and printing with python code.
        # Prepare arguments: one result i32.
        # Arguments must be passed as pointers.
        c_int_p = ctypes.c_int * 1
        res = c_int_p(-1)
        execution_engine.invoke("main", res)

        log("RESULT: ", res[0])
        # CHECK: RESULT: -13


test_min_pooling_builtin()


def test_min_pooling_generic():
    with Context() as ctx, Location.unknown():
        module = Module.create()
        f64 = F64Type.get()
        i32 = IntegerType.get_signless(32)
        with InsertionPoint(module.body):

            @func.FuncOp.from_py_func(
                MemRefType.get((1, 4, 16, 1), f64),
                MemRefType.get((2, 2), f64),
                MemRefType.get((1, 2, 4, 1), i32),
            )
            # Set the strides and use the default dilations.
            def pooling_on_buffers(input, shape, output):
                linalg.pooling_nhwc_min(
                    input, shape, outs=[output], strides=[2, 4], emit_generic=True
                )

        execution_engine = ExecutionEngine(transform(module, pooling_boiler))

        # TODO: FFI-based solution to allow testing and printing with python code.
        # Prepare arguments: one result i32.
        # Arguments must be passed as pointers.
        c_int_p = ctypes.c_int * 1
        res = c_int_p(-1)
        execution_engine.invoke("main", res)

        log("RESULT: ", res[0])
        # CHECK: RESULT: -13


test_min_pooling_generic()
