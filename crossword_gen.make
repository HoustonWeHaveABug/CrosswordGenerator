CROSSWORD_GEN_C_FLAGS=-c -O2 -Wall -Wextra -Waggregate-return -Wcast-align -Wcast-qual -Wconversion -Wformat=2 -Winline -Wlong-long -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -Wno-import -Wpointer-arith -Wredundant-decls -Wshadow -Wstrict-prototypes -Wwrite-strings
CROSSWORD_GEN_OBJS=crossword_gen.o mtrand.o

crossword_gen: ${CROSSWORD_GEN_OBJS}
	gcc -o crossword_gen ${CROSSWORD_GEN_OBJS}

crossword_gen.o: crossword_gen.c crossword_gen.make
	gcc ${CROSSWORD_GEN_C_FLAGS} -o crossword_gen.o crossword_gen.c

mtrand.o: mtrand.h mtrand.c crossword_gen.make
	gcc ${CROSSWORD_GEN_C_FLAGS} -o mtrand.o mtrand.c

clean:
	rm -f crossword_gen ${CROSSWORD_GEN_OBJS}
