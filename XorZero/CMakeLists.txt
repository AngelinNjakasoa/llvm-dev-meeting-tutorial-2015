# LLVM provide a CMake function to build a LLVM Module from sources, use it!
add_llvm_loadable_module(LLVMXorZero XorZero.cpp)

# And it's still possible to use regular CMake linking stuff
target_link_libraries(LLVMXorZero Utils)
