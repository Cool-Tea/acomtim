# acomtim

**A**lgorithms, but **COM**pile-**T**ime **IM**plementation.

Explore the possiblities of compile time computing.

## Requisite

Any c++ compilers that support c++23 standard:
- g++15 or above
- clang++20 or above
- msvc (idk cause i dont use msvc)

## Building

Just take the source code and compile it with c++23 standard.

## Sieve Primes

Implement the sieve of Erathostenes, inspired by [http://cryp.to/prime-sieve/](http://cryp.to/prime-sieve/).

**Located at**: [sieve_primes.cpp](./src/sieve_primes.cpp)

## Parse Command Line Arguments

**NOTICE: THIS IMPLEMENTATION REQUIRES C++26 REFLECTION SUPPORT!!!**

Implement the command line argument parser (or clap), inspired by [p3394](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p3394r4.html#command-line-argument-parsing). However, due to the experimental feature of c++26, the implementation is not fully functional.

**Located at**: [clap.cpp](./src/clap.cpp)
**Toolchain**: gcc 16.1.0
