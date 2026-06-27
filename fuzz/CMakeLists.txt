# libFuzzer harness for the ITCH parser. Built only when EIB_FUZZ=ON (clang).
# eib_core is already instrumented with fuzzer-no-link + ASan/UBSan by the parent
# CMakeLists, so coverage feedback reaches the parser code.
add_executable(fuzz_itch fuzz_itch.cpp)
target_link_libraries(fuzz_itch PRIVATE eib_core)
target_compile_options(fuzz_itch PRIVATE -fsanitize=fuzzer,address,undefined)
target_link_options(fuzz_itch PRIVATE -fsanitize=fuzzer,address,undefined)
