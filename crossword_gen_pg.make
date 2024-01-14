CROSSWORD_GEN_PG_C_FLAGS=-c -pg -std=c89 -Wpedantic -Wall -Wextra -Waggregate-return -Wcast-align -Wcast-qual -Wconversion -Wformat=2 -Winline -Wlong-long -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -Wpointer-arith -Wredundant-decls -Wshadow -Wstrict-prototypes -Wwrite-strings -Wswitch-default -Wswitch-enum -Wbad-function-cast -Wstrict-overflow=5 -Wundef -Wlogical-op -Wfloat-equal -Wold-style-definition
CROSSWORD_GEN_PG_OBJS=crossword_gen_pg.o mtrand_pg.o

crossword_gen_pg: ${CROSSWORD_GEN_PG_OBJS}
	gcc -pg -o crossword_gen_pg ${CROSSWORD_GEN_PG_OBJS}

crossword_gen_pg.o: crossword_gen.c crossword_gen_pg.make
	gcc ${CROSSWORD_GEN_PG_C_FLAGS} -o crossword_gen_pg.o crossword_gen.c

mtrand_pg.o: mtrand.h mtrand.c crossword_gen_pg.make
	gcc ${CROSSWORD_GEN_PG_C_FLAGS} -o mtrand_pg.o mtrand.c

clean:
	rm -f crossword_gen_pg ${CROSSWORD_GEN_PG_OBJS}
