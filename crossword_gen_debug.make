CROSSWORD_GEN_DEBUG_C_FLAGS=-c -g -std=c89 -Wpedantic -Wall -Wextra -Waggregate-return -Wcast-align -Wcast-qual -Wconversion -Wformat=2 -Winline -Wlong-long -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -Wpointer-arith -Wredundant-decls -Wshadow -Wstrict-prototypes -Wwrite-strings -Wswitch-default -Wswitch-enum -Wbad-function-cast -Wstrict-overflow=5 -Wundef -Wlogical-op -Wfloat-equal -Wold-style-definition
CROSSWORD_GEN_DEBUG_OBJS=crossword_gen_debug.o mtrand_debug.o

crossword_gen_debug: ${CROSSWORD_GEN_DEBUG_OBJS}
	gcc -g -o crossword_gen_debug ${CROSSWORD_GEN_DEBUG_OBJS}

crossword_gen_debug.o: crossword_gen.c crossword_gen_debug.make
	gcc ${CROSSWORD_GEN_DEBUG_C_FLAGS} -o crossword_gen_debug.o crossword_gen.c

mtrand_debug.o: mtrand.h mtrand.c crossword_gen_debug.make
	gcc ${CROSSWORD_GEN_DEBUG_C_FLAGS} -o mtrand_debug.o mtrand.c

clean:
	rm -f crossword_gen_debug ${CROSSWORD_GEN_DEBUG_OBJS}
