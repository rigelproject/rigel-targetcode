Targetcode
==========

The targetcode repository contains C and assembly source code intended to run on
the target (Rigel). This includes libraries and application code.

### ./include

Rigel standard headers, which are installed to ${RIGEL_INSTALL}/target/include/

### ./lib

./lib includes:
* a newlib 1.19-based standard C library
* libpthread, a port of a subset of POSIX Threads thread management, thread-local
storage, mutexes, and condition variables
* compiler-rt, an LLVM project providing the runtime functions the compiler uses
to support double-precision floating point, 64-bit integer arithmetic, C99
complex arithmetic, and unaligned loads and stores. We have also adapted
compiler-rt's implementation of Blocks, a language extension to C, Objective-C,
and C++ that implements a type of closure.
* libpar, a library implementing the Rigel Task Model (RTM) task-based programming model
* taskmodel-x86, an incomplete pthreads port of the RTM programming model to
enable faster development or validation of RTM-based programs via execution on the host machine
* linker, a place to put linker scripts for laying out and aligning Rigel binaries
* rigel-crt0-mp.S, the startup code run by all Rigel binaries before jumping into
newlib's pre-main() setup code. The primary function of this file is to set
stack pointers for all threads in the system.

### ./src

./src includes :

* a small set of data- and task-parallel benchmarks 
* test codes useful for evaluating the correctness and performance of library and
compiler features
* Makefile.common with environment variables and compiler toolchain flags. 

Currently included benchmarks are:

* *fft*, a 2D Danielson-Lanczos-based Complex-to-Complex Fast Fourier Transform.
This benchmark comes with a test data and golden output generator, fft_host,
based on FFTW3. The simulator's output can be checked against the golden output
using fpdiff, a tool for comparing binary files of single-precision floats with
user-specified tolerance.

* *march*, an implementation of Marching Cubes scalar field isosurface
polygonization. This parallel implementation is based on a by Paul Bourke and
Cory Bloyd. This benchmark comes with a tool to convert volume data as found in
the Stanford volume data archive to a binary format suitable for Rigel, as well
as a tool to convert RigelSim's list of output triangles into a .obj file
suitable for viewing. Our sample data set, a CT image of the head, comes from
the UNC Volume Rendering Test Data Set by way of the Stanford volume data
archive.

* *stencil*, a 3D 7-point stencil computation modeling, for example, heat diffusion
over time in 3D.