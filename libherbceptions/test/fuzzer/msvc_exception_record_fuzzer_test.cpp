// REQUIRES: windows
// RUN: %cxx %herbceptions_flags -I%herbceptions_include -I%herbceptions_src/src -L%herbceptions_lib -lherbceptions %herbceptions_src/fuzz/msvc_exception_record_fuzzer.cpp -o %t && echo "HE" | %t
int main(){return 0;}
