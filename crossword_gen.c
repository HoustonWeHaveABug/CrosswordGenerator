#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>
#include "mtrand.h"

#define HALF_BITS 4
#define OPTION_SYM_BLACKS 1
#define OPTION_CONNECTED_WHITES 2
#define OPTION_LINEAR_BLACKS 4
#define SYMBOL_BLACK '#'
#define SYMBOL_UNKNOWN '.'
#define SYMBOL_WHITE '*'

typedef enum {
	HEURISTIC_FREQUENCY,
	HEURISTIC_RANDOM
}
heuristic_t;

typedef struct node_s node_t;
typedef struct cell_s cell_t;

typedef struct {
	int symbol;
	node_t *next;
	int leaves_n;
	int len_min;
	int len_max;
}
letter_t;

struct node_s {
	int letters_n;
	letter_t *letters;
};

struct cell_s {
	int row;
	int col;
	letter_t *letter_hor;
	letter_t *letter_ver;
	int symbol;
	int hor_whites_max;
	int ver_whites_max;
	cell_t *sym180;
	cell_t *sym90;
	int visited;
};

typedef struct {
	letter_t *letter_hor;
	letter_t *letter_ver;
	int leaves_n;
}
choice_t;

static void expected_parameters(void);
static int load_dictionary(const char *);
static node_t *get_node_next(node_t *, int);
static node_t *set_letter(letter_t *, int);
static node_t *new_node(void);
static void sort_node(letter_t *, const node_t *);
static void sort_child(letter_t *, letter_t *);
static int compare_letters(const void *, const void *);
static void set_row(cell_t *, int, letter_t *, letter_t *, int);
static void set_cell(cell_t *, int, int, letter_t *, letter_t *, int);
static int solve_grid(cell_t *);
static int solve_cell(cell_t *, const node_t *, const node_t *, int);
static int check_letters(const letter_t *, const letter_t *);
static int check_letter(const letter_t *);
static int add_choice(letter_t *, letter_t *);
static void set_choice(choice_t *, letter_t *, letter_t *);
static int multiply_ints(int, int);
static int compare_choices(const void *, const void *);
static void copy_choice(cell_t *, choice_t *);
static int solve_end_cell(letter_t *, cell_t *);
static int are_whites_connected(int);
static void add_cell_to_queue(cell_t *);
static void free_node(node_t *);

static int short_bits, cells_max, short_max, rows_n, cols_n, blacks_min, blacks_max, choices_max, sym_blacks, connected_whites, linear_blacks, cols_total, choices_size, *black_counts, *blacks2_in_cols, cells_n, blacks_n1, choices_hi, symmetric, pos, blacks_n2, whites_n1, whites_n3, blacks_n3, overflow, hor_whites_min, hor_whites_max, ver_whites_min, ver_whites_max, choices_n, queued_cells_n;
static double blacks_ratio;
static heuristic_t heuristic;
static letter_t letter_root;
static node_t *node_root;
static cell_t *cells, **queued_cells, *first_white;
static choice_t *choices;

int main(int argc, char *argv[]) {
	int options, r, i;
	unsigned long mtseed;
	short_bits = (int)sizeof(int)*HALF_BITS;
	cells_max = 1 << short_bits;
	short_max = cells_max-1;
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <dictionary>\n", argv[0]);
		expected_parameters();
		return EXIT_FAILURE;
	}
	if (scanf("%d%d%d%d%u%d%d", &rows_n, &cols_n, &blacks_min, &blacks_max, &heuristic, &choices_max, &options) != 7 || rows_n < 1 || rows_n > cols_n || rows_n > cells_max/cols_n || blacks_min < 0 || blacks_min > blacks_max || blacks_max > rows_n*cols_n || choices_max < 1) {
		fputs("Invalid grid settings\n", stderr);
		expected_parameters();
		return EXIT_FAILURE;
	}
	sym_blacks = options & OPTION_SYM_BLACKS;
	connected_whites = options & OPTION_CONNECTED_WHITES;
	linear_blacks = options & OPTION_LINEAR_BLACKS;
	node_root = new_node();
	if (!node_root) {
		return EXIT_FAILURE;
	}
	if (!load_dictionary(argv[1])) {
		free_node(node_root);
		return EXIT_FAILURE;
	}
	letter_root.symbol = SYMBOL_BLACK;
	letter_root.next = node_root;
	sort_node(&letter_root, node_root);
	letter_root.leaves_n -= rows_n+cols_n;
	cols_total = cols_n+2;
	cells = malloc(sizeof(cell_t)*(size_t)((rows_n+2)*cols_total));
	if (!cells) {
		fputs("Could not allocate memory for cells\n", stderr);
		fflush(stderr);
		free_node(node_root);
		return EXIT_FAILURE;
	}
	set_row(cells, -1, NULL, &letter_root, SYMBOL_BLACK);
	for (i = 1; i <= rows_n; ++i) {
		set_row(cells+i*cols_total, i-1, &letter_root, NULL, SYMBOL_UNKNOWN);
	}
	set_row(cells+i*cols_total, rows_n, NULL, NULL, SYMBOL_BLACK);
	choices = malloc(sizeof(choice_t));
	if (!choices) {
		fputs("Could not allocate memory for choices\n", stderr);
		fflush(stderr);
		free(cells);
		free_node(node_root);
		return EXIT_FAILURE;
	}
	choices_size = 1;
	black_counts = malloc(sizeof(int)*(size_t)(rows_n+cols_n));
	if (!black_counts) {
		fputs("Could not allocate memory for black_counts\n", stderr);
		fflush(stderr);
		free(choices);
		free(cells);
		free_node(node_root);
		return EXIT_FAILURE;
	}
	for (i = rows_n; i--; ) {
		black_counts[i] = (rows_n-i-1)/(letter_root.len_max+1);
	}
	blacks2_in_cols = black_counts+rows_n;
	for (i = cols_n; i--; ) {
		blacks2_in_cols[i] = 0;
	}
	cells_n = rows_n*cols_n;
	queued_cells = malloc(sizeof(cell_t *)*(size_t)cells_n);
	if (!queued_cells) {
		fputs("Could not allocate memory for queued_cells\n", stderr);
		fflush(stderr);
		free(black_counts);
		free(choices);
		free(cells);
		free_node(node_root);
		return EXIT_FAILURE;
	}
	blacks_n1 = 0;
	choices_hi = 0;
	symmetric = rows_n == cols_n;
	pos = 0;
	blacks_n2 = black_counts[0]*cols_n;
	whites_n1 = 0;
	whites_n3 = 0;
	blacks_n3 = 0;
	blacks_ratio = (double)blacks_max/cells_n;
	if (scanf("%lu", &mtseed) != 1) {
		mtseed = (unsigned long)time(NULL);
	}
	do {
		printf("CHOICES %d\n", choices_max);
		fflush(stdout);
		smtrand(mtseed);
		overflow = 0;
		r = solve_grid(cells+cols_total+1);
		++choices_max;
	}
	while (overflow && !r);
	free(queued_cells);
	free(black_counts);
	free(choices);
	free(cells);
	free_node(node_root);
	return EXIT_SUCCESS;
}

static void expected_parameters(void) {
	fputs("Parameters expected on the standard input:\n", stderr);
	fputs("- Number of rows (> 0)\n", stderr);
	fprintf(stderr, "- Number of columns (>= Number of rows, Number of cells <= %d)\n", cells_max);
	fputs("- Minimum number of black squares (>= 0)\n", stderr);
	fputs("- Maximum number of black squares (>= Minimum number of black squares, <= Number of cells)\n", stderr);
	fprintf(stderr, "- Heuristic (%u: frequency, %u: random, > %u: none)\n", HEURISTIC_FREQUENCY, HEURISTIC_RANDOM, HEURISTIC_RANDOM);
	fputs("- Maximum number of choices at each step (> 0)\n", stderr);
	fputs("- Options (= sum of the below flags)\n", stderr);
	fprintf(stderr, "\t- Black squares symmetry (0: disabled, %d: enabled)\n", OPTION_SYM_BLACKS);
	fprintf(stderr, "\t- White squares connected (0: disabled, %d: enabled)\n", OPTION_CONNECTED_WHITES);
	fprintf(stderr, "\t- Linear blacks (0: disabled, %d: enabled)\n", OPTION_LINEAR_BLACKS);
	fputs("- [ RNG seed ]\n", stderr);
	fflush(stderr);
}

static int load_dictionary(const char *fn) {
	int *symbols = malloc(sizeof(int)*(size_t)(cols_n+1)), line, len, c;
	node_t *node;
	FILE *fd;
	if (!symbols) {
		fputs("Could not allocate memory for symbols\n", stderr);
		fflush(stderr);
		return 0;
	}
	fd = fopen(fn, "r");
	if (!fd) {
		fputs("Could not open the dictionary\n", stderr);
		fflush(stderr);
		free(symbols);
		return 0;
	}
	node = node_root;
	line = 1;
	len = 0;
	c = fgetc(fd);
	while (!feof(fd)) {
		c = toupper(c);
		if (isupper(c)) {
			if (len < cols_n) {
				symbols[len] = c;
			}
			++len;
		}
		else if (c == '\n') {
			if (((blacks_max && len <= cols_n) || len == rows_n || len == cols_n)) {
				int i;
				symbols[len] = SYMBOL_BLACK;
				for (i = 0; i <= len; ++i) {
					node = get_node_next(node, symbols[i]);
					if (!node) {
						fclose(fd);
						free(symbols);
						return 0;
					}
				}
			}
			++line;
			len = 0;
		}
		else {
			fprintf(stderr, "Invalid character %c in dictionary on line %d\n", c, line);
			fflush(stderr);
			fclose(fd);
			free(symbols);
			return 0;
		}
		c = fgetc(fd);
	}
	fclose(fd);
	free(symbols);
	if (len) {
		fputs("Unexpected end of dictionary\n", stderr);
		fflush(stderr);
		return 0;
	}
	return !blacks_max || get_node_next(node_root, SYMBOL_BLACK);
}

static node_t *get_node_next(node_t *node, int symbol) {
	int i;
	node_t *next;
	for (i = node->letters_n; i--; ) {
		if (node->letters[i].symbol == symbol) {
			return node->letters[i].next;
		}
	}
	if (node->letters_n) {
		letter_t *letters = realloc(node->letters, sizeof(letter_t)*(size_t)(node->letters_n+1));
		if (!letters) {
			fputs("Could not reallocate memory for node->letters\n", stderr);
			fflush(stderr);
			return NULL;
		}
		node->letters = letters;
	}
	else {
		node->letters = malloc(sizeof(letter_t));
		if (!node->letters) {
			fputs("Could not allocate memory for node->letters\n", stderr);
			fflush(stderr);
			return NULL;
		}
	}
	next = set_letter(node->letters+node->letters_n, symbol);
	++node->letters_n;
	return next;
}

static node_t *set_letter(letter_t *letter, int symbol) {
	letter->symbol = symbol;
	letter->next = symbol != SYMBOL_BLACK ? new_node():node_root;
	return letter->next;
}

static node_t *new_node(void) {
	node_t *node = malloc(sizeof(node_t));
	if (!node) {
		fputs("Could not allocate memory for node\n", stderr);
		fflush(stderr);
		return NULL;
	}
	node->letters_n = 0;
	return node;
}

static void sort_node(letter_t *letter, const node_t *node) {
	int i;
	letter->leaves_n = 0;
	letter->len_min = cols_n;
	letter->len_max = 0;
	for (i = node->letters_n; i--; ) {
		sort_child(letter, node->letters+i);
	}
	qsort(node->letters, (size_t)node->letters_n, sizeof(letter_t), compare_letters);
}

static void sort_child(letter_t *letter, letter_t *child) {
	if (child->next != node_root) {
		sort_node(child, child->next);
		++child->len_min;
		++child->len_max;
	}
	else {
		child->leaves_n = letter != &letter_root ? 1:blacks_max*2+rows_n+cols_n;
		child->len_min = 0;
		child->len_max = 0;
	}
	letter->leaves_n += child->leaves_n;
	if (child->len_min < letter->len_min) {
		letter->len_min = child->len_min;
	}
	if (child->len_max > letter->len_max) {
		letter->len_max = child->len_max;
	}
}

static int compare_letters(const void *a, const void *b) {
	return ((const letter_t *)a)->symbol-((const letter_t *)b)->symbol;
}

static void set_row(cell_t *first, int row, letter_t *letter_hor, letter_t *letter_ver, int symbol) {
	int i;
	set_cell(first, row, -1, letter_hor, NULL, SYMBOL_BLACK);
	for (i = 1; i <= cols_n; ++i) {
		set_cell(first+i, row, i-1, NULL, letter_ver, symbol);
	}
	set_cell(first+i, row, cols_n, NULL, NULL, SYMBOL_BLACK);
}

static void set_cell(cell_t *cell, int row, int col, letter_t *letter_hor, letter_t *letter_ver, int symbol) {
	cell->row = row;
	cell->col = col;
	cell->letter_hor = letter_hor;
	cell->letter_ver = letter_ver;
	cell->symbol = symbol;
	cell->hor_whites_max = cols_n-col;
	cell->ver_whites_max = rows_n-row;
	cell->sym180 = cells+(rows_n-row)*cols_total+cols_n-col;
	cell->sym90 = cells+(col+1)*cols_total+row+1;
	cell->visited = 0;
}

static int solve_grid(cell_t *cell) {
	if (cell->row < rows_n) {
		if (cell->col < cols_n) {
			return solve_cell(cell, (cell-1)->letter_hor->next, (cell-cols_total)->letter_ver->next, choices_hi);
		}
		return solve_end_cell((cell-1)->letter_hor->next->letters, cell+2);
	}
	if (cell->col < cols_n) {
		return solve_end_cell((cell-cols_total)->letter_ver->next->letters, cell+1);
	}
	if (are_whites_connected(whites_n1)) {
		int i;
		blacks_max = blacks_n1-1;
		blacks_ratio = (double)blacks_max/cells_n;
		printf("BLACK SQUARES %d\n", blacks_n1);
		for (i = 1; i <= rows_n; ++i) {
			int j;
			putchar(cells[i*cols_total+1].symbol);
			for (j = 2; j <= cols_n; ++j) {
				printf(" %c", cells[i*cols_total+j].symbol);
			}
			puts("");
		}
		fflush(stdout);
		return blacks_min > blacks_max;
	}
	return 0;
}

static int solve_cell(cell_t *cell, const node_t *node_hor, const node_t *node_ver, int choices_lo) {
	int symmetric_bak, blacks2_in_col, r, i, j;
	if (sym_blacks) {
		cell_t *cell_sym;
		for (cell_sym = cell->sym180; cell_sym->symbol != SYMBOL_UNKNOWN && cell_sym->symbol != SYMBOL_BLACK; --cell_sym);
		hor_whites_min = cell->sym180->col-cell_sym->col;
		for (; cell_sym->symbol != SYMBOL_BLACK; --cell_sym);
		hor_whites_max = cell->sym180->col-cell_sym->col;
		for (cell_sym = cell->sym180; cell_sym->symbol != SYMBOL_UNKNOWN && cell_sym->symbol != SYMBOL_BLACK; cell_sym -= cols_total);
		ver_whites_min = cell->sym180->row-cell_sym->row;
		for (; cell_sym->symbol != SYMBOL_BLACK; cell_sym -= cols_total);
		ver_whites_max = cell->sym180->row-cell_sym->row;
	}
	else {
		hor_whites_max = cell->hor_whites_max;
		ver_whites_max = cell->ver_whites_max;
		if (blacks_n1 < blacks_max) {
			hor_whites_min = 0;
			ver_whites_min = 0;
		}
		else {
			hor_whites_min = hor_whites_max;
			ver_whites_min = ver_whites_max;
		}
	}
	i = cell->symbol != SYMBOL_WHITE || node_hor->letters[0].symbol != SYMBOL_BLACK ? 0:1;
	if (symmetric && cell->sym90 < cell) {
		for (; i < node_hor->letters_n && node_hor->letters[i].symbol < cell->sym90->symbol; ++i);
	}
	if (node_hor != node_ver) {
		if (cell->symbol != SYMBOL_BLACK) {
			for (j = 0; i < node_hor->letters_n; ++i) {
				for (; j < node_ver->letters_n && node_ver->letters[j].symbol < node_hor->letters[i].symbol; ++j);
				if (j == node_ver->letters_n) {
					break;
				}
				if (node_ver->letters[j].symbol == node_hor->letters[i].symbol) {
					if (check_letters(node_hor->letters+i, node_ver->letters+j) && !add_choice(node_hor->letters+i, node_ver->letters+j)) {
						return -1;
					}
					++j;
				}
			}
		}
		else {
			if (i < node_hor->letters_n && node_hor->letters[i].symbol == SYMBOL_BLACK && node_ver->letters[0].symbol == SYMBOL_BLACK && check_letters(node_hor->letters, node_ver->letters) && !add_choice(node_hor->letters, node_ver->letters)) {
				return -1;
			}
		}
	}
	else {
		if (cell->symbol != SYMBOL_BLACK) {
			for (; i < node_hor->letters_n; ++i) {
				if (check_letter(node_hor->letters+i) && !add_choice(node_hor->letters+i, node_hor->letters+i)) {
					return -1;
				}
			}
		}
		else {
			if (i < node_hor->letters_n && node_hor->letters[i].symbol == SYMBOL_BLACK && check_letter(node_hor->letters) && !add_choice(node_hor->letters, node_hor->letters)) {
				return -1;
			}
		}
	}
	choices_n = choices_hi-choices_lo;
	if (!choices_n) {
		return 0;
	}
	if (choices_n > 1) {
		if (heuristic == HEURISTIC_FREQUENCY) {
			qsort(choices+choices_lo, (size_t)choices_n, sizeof(choice_t), compare_choices);
		}
		else if (heuristic == HEURISTIC_RANDOM) {
			for (i = choices_lo; i < choices_hi; ++i) {
				choice_t choice_tmp = choices[i];
				j = (int)emtrand((unsigned long)(choices_hi-i))+i;
				choices[i] = choices[j];
				choices[j] = choice_tmp;
			}
		}
	}
	symmetric_bak = symmetric;
	if (linear_blacks) {
		++pos;
	}
	blacks2_in_col = blacks2_in_cols[cell->col];
	blacks_n2 -= blacks2_in_col;
	for (i = choices_lo, j = 0, r = 0; i < choices_hi && j < choices_max && !r; ++i) {
		copy_choice(cell, choices+i);
		if (cell->letter_hor->symbol != SYMBOL_BLACK) {
			blacks2_in_cols[cell->col] = cell->row+cell->letter_ver->len_max < rows_n ? 1+black_counts[cell->row+cell->letter_ver->len_max]:0;
			blacks_n2 += blacks2_in_cols[cell->col];
			if (blacks_n1+blacks_n2 <= blacks_max) {
				if (connected_whites) {
					if (!whites_n1) {
						first_white = cell;
					}
					++whites_n1;
					if (sym_blacks && cell->sym180 > cell) {
						++whites_n3;
					}
				}
				cell->symbol = cell->letter_hor->symbol;
				if (sym_blacks && cell->sym180 > cell) {
					cell->sym180->symbol = SYMBOL_WHITE;
				}
				--cell->letter_hor->leaves_n;
				--cell->letter_ver->leaves_n;
				if (symmetric_bak && cell->sym90 < cell) {
					symmetric = cell->symbol == cell->sym90->symbol;
				}
				r = solve_grid(cell+1);
				++j;
				++cell->letter_ver->leaves_n;
				++cell->letter_hor->leaves_n;
				if (sym_blacks && cell->sym180 > cell) {
					cell->sym180->symbol = SYMBOL_UNKNOWN;
				}
				cell->symbol = !sym_blacks || cell->sym180 >= cell ? SYMBOL_UNKNOWN:SYMBOL_WHITE;
				if (connected_whites) {
					if (sym_blacks && cell->sym180 > cell) {
						--whites_n3;
					}
					--whites_n1;
				}
			}
		}
		else {
			blacks2_in_cols[cell->col] = black_counts[cell->row];
			blacks_n2 += blacks2_in_cols[cell->col];
			++blacks_n1;
			if (sym_blacks) {
				if (cell->sym180 > cell) {
					++blacks_n3;
				}
				else if (cell->sym180 < cell) {
					--blacks_n3;
				}
			}
			if (blacks_n1+blacks_n2 <= blacks_max && (!sym_blacks || blacks_n1+blacks_n3 <= blacks_max) && (!linear_blacks || (double)blacks_n1 <= blacks_ratio*pos)) {
				if (!sym_blacks || cell->sym180 >= cell) {
					cell->symbol = SYMBOL_BLACK;
				}
				if (sym_blacks && cell->sym180 > cell) {
					cell->sym180->symbol = SYMBOL_BLACK;
				}
				if (are_whites_connected(whites_n1)) {
					--cell->letter_hor->leaves_n;
					--cell->letter_ver->leaves_n;
					if (symmetric_bak && cell->sym90 < cell) {
						symmetric = cell->symbol == cell->sym90->symbol;
					}
					r = solve_grid(cell+1);
					++j;
					++cell->letter_ver->leaves_n;
					++cell->letter_hor->leaves_n;
				}
				if (sym_blacks && cell->sym180 > cell) {
					cell->sym180->symbol = SYMBOL_UNKNOWN;
				}
				if (!sym_blacks || cell->sym180 >= cell) {
					cell->symbol = SYMBOL_UNKNOWN;
				}
			}
			if (sym_blacks) {
				if (cell->sym180 > cell) {
					--blacks_n3;
				}
				else if (cell->sym180 < cell) {
					++blacks_n3;
				}
			}
			--blacks_n1;
		}
		blacks_n2 -= blacks2_in_cols[cell->col];
	}
	blacks_n2 += blacks2_in_col;
	blacks2_in_cols[cell->col] = blacks2_in_col;
	if (linear_blacks) {
		--pos;
	}
	symmetric = symmetric_bak;
	overflow |= i < choices_hi;
	choices_hi = choices_lo;
	return r;
}

static int check_letters(const letter_t *letter_hor, const letter_t *letter_ver) {
	return letter_hor->leaves_n && letter_hor->len_min <= hor_whites_max && letter_hor->len_max >= hor_whites_min && letter_ver->leaves_n && letter_ver->len_min <= ver_whites_max && letter_ver->len_max >= ver_whites_min;
}

static int check_letter(const letter_t *letter) {
	return letter->leaves_n > 1 && letter->len_min <= hor_whites_max && letter->len_max >= hor_whites_min && letter->len_min <= ver_whites_max && letter->len_max >= ver_whites_min;
}

static int add_choice(letter_t *letter_hor, letter_t *letter_ver) {
	if (choices_hi == choices_size) {
		choice_t *choices_tmp = realloc(choices, sizeof(choice_t)*(size_t)(choices_hi+1));
		if (!choices_tmp) {
			fputs("Could not reallocate memory for choices\n", stderr);
			fflush(stderr);
			return 0;
		}
		choices = choices_tmp;
		choices_size = choices_hi+1;
	}
	set_choice(choices+choices_hi, letter_hor, letter_ver);
	++choices_hi;
	return 1;
}

static void set_choice(choice_t *choice, letter_t *letter_hor, letter_t *letter_ver) {
	choice->letter_hor = letter_hor;
	choice->letter_ver = letter_ver;
	if (heuristic == HEURISTIC_FREQUENCY) {
		choice->leaves_n = letter_hor != letter_ver ? multiply_ints(letter_hor->leaves_n, letter_ver->leaves_n):letter_hor->leaves_n;
	}
}

static int multiply_ints(int a, int b) {
	if (a <= INT_MAX/b) {
		return a*b;
	}
	return INT_MAX;
}

static int compare_choices(const void *a, const void *b) {
	const choice_t *choice_a = (const choice_t *)a, *choice_b = (const choice_t *)b;
	if (choice_a->leaves_n != choice_b->leaves_n) {
		return choice_b->leaves_n-choice_a->leaves_n;
	}
	return choice_a->letter_hor->symbol-choice_b->letter_hor->symbol;
}

static void copy_choice(cell_t *cell, choice_t *choice) {
	cell->letter_hor = choice->letter_hor;
	cell->letter_ver = choice->letter_ver;
}

static int solve_end_cell(letter_t *letter, cell_t *cell) {
	if (letter->symbol == SYMBOL_BLACK && letter->leaves_n) {
		int r;
		--letter->leaves_n;
		r = solve_grid(cell);
		++letter->leaves_n;
		return r;
	}
	return 0;
}

static int are_whites_connected(int target) {
	int i;
	if (!target || (sym_blacks && blacks_n1 > blacks_n3+2 && whites_n1 > whites_n3)) {
		return 1;
	}
	queued_cells_n = 0;
	add_cell_to_queue(first_white);
	for (i = 0; i < queued_cells_n; ++i) {
		if (isupper(queued_cells[i]->symbol)) {
			--target;
			if (!target) {
				break;
			}
		}
		add_cell_to_queue(queued_cells[i]+1);
		add_cell_to_queue(queued_cells[i]+cols_total);
		add_cell_to_queue(queued_cells[i]-1);
		add_cell_to_queue(queued_cells[i]-cols_total);
	}
	for (i = queued_cells_n; i--; ) {
		queued_cells[i]->visited = 0;
	}
	return !target;
}

static void add_cell_to_queue(cell_t *cell) {
	if (cell->symbol != SYMBOL_BLACK && !cell->visited) {
		cell->visited = 1;
		queued_cells[queued_cells_n++] = cell;
	}
}

static void free_node(node_t *node) {
	if (node->letters_n) {
		int i;
		for (i = node->letters_n; i--; ) {
			if (node->letters[i].next != node_root) {
				free_node(node->letters[i].next);
			}
		}
		free(node->letters);
	}
	free(node);
}
