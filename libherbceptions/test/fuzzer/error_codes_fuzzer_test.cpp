// RUN: %cxx %herbceptions_flags -I%herbceptions_include -I%herbceptions_src/src -L%herbceptions_lib -lherbceptions %herbceptions_src/fuzz/error_codes_fuzzer.cpp -o %t && echo "testinput" | %t
int main(){return 0;}
