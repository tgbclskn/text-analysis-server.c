tas:
	gcc -Wall -Wextra -fanalyzer -s -march=native -mtune=native -Ofast -fipa-pta -fgraphite-identity -floop-nest-optimize -fdevirtualize-at-ltrans -fwhole-program -flto=auto -pipe -DDEBUG2 -o tas tas.c tas_server.c levenshtein.c

.PHONY: tas clean

clean:
	rm tas

debug:
	gcc -Wall -Wextra -s -march=native -mtune=native -Ofast -fipa-pta -fgraphite-identity -floop-nest-optimize -fdevirtualize-at-ltrans -fwhole-program -flto=auto -pipe -DDEBUG -o tas tas.c tas_server.c levenshtein.c

debug2:
	gcc -Wall -Wextra -s -march=native -mtune=native -Ofast -fipa-pta -fgraphite-identity -floop-nest-optimize -fdevirtualize-at-ltrans -fwhole-program -flto=auto -pipe -DDEBUG2 -o tas tas.c tas_server.c levenshtein.c

debug3:
	gcc -Wall -Wextra -g -march=native -mtune=native -pipe -DDEBUG2 -o tas tas.c tas_server.c levenshtein.c

tass:
	gcc -Wall -Wextra -fanalyzer -fsanitize=address,undefined,leak -g3 -march=native -mtune=native -Og -pipe -DDEBUG2 -o tas tas.c tas_server.c levenshtein.c
