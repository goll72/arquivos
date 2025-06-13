fuzz_b_tree: fuzz_b_tree.c ../src/index/b_tree.c
	clang -g -O0 -fno-omit-frame-pointer -fsanitize=fuzzer,address -I ../include $^ -o $@ 
