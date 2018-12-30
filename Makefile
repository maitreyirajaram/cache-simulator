all: foo
foo: first.c
	gcc -g -Wall -Werror -fsanitize=address -std=c99 -lm  first.c -o first

clean: 
	rm -rf first
