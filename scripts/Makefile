fuzz_b_tree: fuzz_b_tree.c ../src/index/b_tree.c
	clang -g -O2 -fsanitize=fuzzer -fprofile-instr-generate -fcoverage-mapping -I ../include $^ -o $@

dump_b_tree: dump_b_tree.c
	clang $^ -o $@
