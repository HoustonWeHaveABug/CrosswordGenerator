CROSSWORD_GEN_DEBUG_C_FLAGS=-c -g -Wall -Wextra -Waggregate-return -Wcast-align -Wcast-qual -Wconversion -Wformat=2 -Winline -Wlong-long -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -Wno-import -Wpointer-arith -Wredundant-decls -Wshadow -Wstrict-prototypes -Wwrite-strings
CROSSWORD_GEN_DEBUG_OBJS=crossword_gen_debug.o mtrand_debug.o

crossword_gen_debug: ${CROSSWORD_GEN_DEBUG_OBJS}
	gcc -g -o crossword_gen_debug ${CROSSWORD_GEN_DEBUG_OBJS}

crossword_gen_debug.o: crossword_gen.c crossword_gen_debug.make
	gcc ${CROSSWORD_GEN_DEBUG_C_FLAGS} -o crossword_gen_debug.o crossword_gen.c

mtrand_debug.o: mtrand.h mtrand.c crossword_gen_debug.make
	gcc ${CROSSWORD_GEN_DEBUG_C_FLAGS} -o mtrand_debug.o mtrand.c

clean:
	rm -f crossword_gen_debug ${CROSSWORD_GEN_DEBUG_OBJS}
