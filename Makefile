all: cache
cache: cache.c
	gcc -g -Wall -Werror -fsanitize=address -std=c99 -lm  cache.c -o cache

clean: 
	rm -rf cache
