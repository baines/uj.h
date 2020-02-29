fuzz: fuzz.c ../../uj.h
	afl-gcc -g -D_GNU_SOURCE -fprofile-arcs -ftest-coverage $< -o $@

normal: fuzz.c ../../uj.h
	gcc -O0 -g -D_GNU_SOURCE $< -o $@

run: fuzz | output
	afl-fuzz -i tests -o output ./$<

cov: fuzz | output
	afl-cov -d output --coverage-cmd 'cat AFL_FILE | ./fuzz' --code-dir . --enable-branch-coverage --overwrite

output:
	mkdir -p $@

.PHONY: run cov normal
