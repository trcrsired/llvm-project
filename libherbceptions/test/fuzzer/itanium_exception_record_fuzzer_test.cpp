// REQUIRES: linux || darwin
// RUN: %cxx %herbceptions_flags -I%herbceptions_include -I%herbceptions_src/src -L%herbceptions_lib -lherbceptions %herbceptions_src/fuzz/itanium_exception_record_fuzzer.cpp -o %t && echo "IT" | %t
int main(){return 0;}
