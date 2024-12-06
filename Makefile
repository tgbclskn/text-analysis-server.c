tas:
	gcc -s -march=native -mtune=native -O3 -o tas tas.c levenshtein.c

.PHONY: tas clean

clean:
	rm tas

