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
	int blacks_in_col;
	int pos;
	int visited;
};

typedef struct {
	letter_t *letter_hor;
	letter_t *letter_ver;
	int leaves_hi;
	int leaves_lo;
}
choice_t;

static void expected_parameters(int);
static int load_dictionary(FILE *);
static int new_word(int *, int);
static letter_t *get_letter(node_t *, int);
static letter_t *new_letter(node_t *, int);
static node_t *new_node(void);
static void sort_node(letter_t *, node_t *);
static void sort_child(letter_t *, letter_t *);
static int compare_letters(const void *, const void *);
static void set_row(cell_t *, int, letter_t *, letter_t *, int);
static void set_cell(cell_t *, int, int, letter_t *, letter_t *, int);
static void set_cell_whites_max(cell_t *);
static int solve_grid(cell_t *);
static int solve_grid_inter(node_t *, cell_t *);
static int check_letter1(letter_t *, int, int);
static int check_letter2(letter_t *, int, int, int, int);
static int add_choice(int, letter_t *, letter_t *);
static void set_choice(choice_t *, letter_t *, letter_t *);
static void hilo_multiply(int, int, int *, int *);
static void hilo_add(int, int, int *, int *);
static int compare_choices(const void *, const void *);
static void copy_choice(cell_t *, choice_t *);
static void roll_cell(cell_t *, int);
static void unroll_cell(cell_t *);
static int solve_grid_final(letter_t *, cell_t *);
static int are_whites_connected(cell_t *);
static void add_cell_to_queue(cell_t *);
static void free_node(node_t *);

static int short_bits, short_max, rows_n, cols_n, blacks_min, blacks_max, choices_max, sym_blacks, connected_whites, linear_blacks, cols_total, choices_size, cells_n, unknown_cells_n, choices_hi, symmetric, blacks_n1, blacks_n2, blacks_n3, overflow, queued_cells_n;
static double blacks_ratio;
static heuristic_t heuristic;
static letter_t letter_root;
static node_t *node_root;
static cell_t *cells, **queued_cells, *first_white;
static choice_t *choices;

int main(int argc, char *argv[]) {
	int cells_max, options, r, i;
	FILE *fd;
	time_t mtseed;
	short_bits = (int)sizeof(int)*HALF_BITS;
	cells_max = 1 << short_bits;
	short_max = cells_max-1;
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <dictionary>\n", argv[0]);
		expected_parameters(cells_max);
		return EXIT_FAILURE;
	}
	if (scanf("%d%d%d%d%u%d%d", &rows_n, &cols_n, &blacks_min, &blacks_max, &heuristic, &choices_max, &options) != 7 || rows_n < 1 || rows_n > cols_n || rows_n > cells_max/cols_n || blacks_min < 0 || blacks_min > blacks_max || blacks_max > rows_n*cols_n || choices_max < 1) {
		fputs("Invalid grid settings\n", stderr);
		expected_parameters(cells_max);
		return EXIT_FAILURE;
	}
	sym_blacks = options & OPTION_SYM_BLACKS;
	connected_whites = options & OPTION_CONNECTED_WHITES;
	linear_blacks = options & OPTION_LINEAR_BLACKS;
	node_root = new_node();
	if (!node_root) {
		return 0;
	}
	fd = fopen(argv[1], "r");
	if (!fd) {
		fputs("Could not open the dictionary\n", stderr);
		fflush(stderr);
		free_node(node_root);
		return EXIT_FAILURE;
	}
	if (!load_dictionary(fd)) {
		fclose(fd);
		free_node(node_root);
		return EXIT_FAILURE;
	}
	fclose(fd);
	letter_root.symbol = SYMBOL_BLACK;
	letter_root.next = node_root;
	sort_node(&letter_root, node_root);
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
	for (i = rows_n; i; --i) {
		int j;
		for (j = cols_n; j; --j) {
			set_cell_whites_max(cells+i*cols_total+j);
		}
	}
	choices = malloc(sizeof(choice_t));
	if (!choices) {
		fputs("Could not allocate memory for choices\n", stderr);
		fflush(stderr);
		free(cells);
		free_node(node_root);
		return EXIT_FAILURE;
	}
	choices_size = 1;
	cells_n = rows_n*cols_n;
	queued_cells = malloc(sizeof(cell_t *)*(size_t)cells_n);
	if (!queued_cells) {
		fputs("Could not allocate memory for queued_cells\n", stderr);
		fflush(stderr);
		free(choices);
		free(cells);
		free_node(node_root);
		return EXIT_FAILURE;
	}
	unknown_cells_n = cells_n;
	choices_hi = 0;
	symmetric = rows_n == cols_n;
	blacks_n1 = 0;
	blacks_n2 = cells[1].blacks_in_col*cols_n;
	blacks_n3 = 0;
	blacks_ratio = (double)blacks_max/cells_n;
	++blacks_max;
	mtseed = time(NULL);
	do {
		printf("CHOICES %d\n", choices_max);
		fflush(stdout);
		smtrand((unsigned long)mtseed);
		overflow = 0;
		r = solve_grid(cells+cols_total+1);
		if (overflow) {
			++choices_max;
		}
	}
	while (overflow && !r);
	free(queued_cells);
	free(choices);
	free(cells);
	free_node(node_root);
	return EXIT_SUCCESS;
}

static void expected_parameters(int cells_max) {
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
	fflush(stderr);
}

static int load_dictionary(FILE *fd) {
	int *symbols = malloc(sizeof(int)*(size_t)cols_n), line, len, c;
	if (!symbols) {
		fputs("Could not allocate memory for symbols\n", stderr);
		fflush(stderr);
		return 0;
	}
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
			if (((blacks_max && len <= cols_n) || len == rows_n || len == cols_n) && !new_word(symbols, len)) {
				free(symbols);
				return 0;
			}
			++line;
			len = 0;
		}
		else {
			fprintf(stderr, "Invalid character %c in dictionary on line %d\n", c, line);
			fflush(stderr);
			free(symbols);
			return 0;
		}
		c = fgetc(fd);
	}
	free(symbols);
	if (len) {
		fputs("Unexpected end of dictionary\n", stderr);
		fflush(stderr);
		return 0;
	}
	return !blacks_max || get_letter(node_root, SYMBOL_BLACK) || new_letter(node_root, SYMBOL_BLACK);
}

static int new_word(int *symbols, int len) {
	int i;
	node_t *node = node_root;
	for (i = 0; i < len; ++i) {
		letter_t *letter = get_letter(node, symbols[i]);
		if (!letter) {
			letter = new_letter(node, symbols[i]);
			if (!letter) {
				return 0;
			}
		}
		node = letter->next;
	}
	return get_letter(node, SYMBOL_BLACK) || new_letter(node, SYMBOL_BLACK);
}

static letter_t *get_letter(node_t *node, int symbol) {
	int i;
	for (i = 0; i < node->letters_n && node->letters[i].symbol != symbol; ++i);
	if (i < node->letters_n) {
		return node->letters+i;
	}
	return NULL;
}

static letter_t *new_letter(node_t *node, int symbol) {
	letter_t *letter;
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
	letter = node->letters+node->letters_n;
	letter->symbol = symbol;
	if (symbol != SYMBOL_BLACK) {
		letter->next = new_node();
		if (!letter->next) {
			return NULL;
		}
	}
	else {
		letter->next = node_root;
	}
	++node->letters_n;
	return letter;
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

static void sort_node(letter_t *letter, node_t *node) {
	letter->leaves_n = 0;
	letter->len_min = cols_n;
	letter->len_max = 0;
	if (node->letters_n) {
		int i;
		for (i = node->letters_n; i--; ) {
			sort_child(letter, node->letters+i);
		}
		qsort(node->letters, (size_t)node->letters_n, sizeof(letter_t), compare_letters);
	}
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
	cell->hor_whites_max = 0;
	cell->ver_whites_max = 0;
	cell->sym180 = cells+(rows_n-row)*cols_total+cols_n-col;
	cell->sym90 = cells+(col+1)*cols_total+row+1;
	cell->blacks_in_col = row >= 0 ? (rows_n-row-1)/(letter_root.len_max+1):(rows_n+row)/(letter_root.len_max+1);
	cell->pos = row*cols_n+col+1;
	cell->visited = 0;
}

static void set_cell_whites_max(cell_t *cell) {
	if (cell->symbol != SYMBOL_BLACK) {
		cell->hor_whites_max = (cell+1)->hor_whites_max+1;
		cell->ver_whites_max = (cell+cols_total)->ver_whites_max+1;
	}
}

static int solve_grid(cell_t *cell) {
	if (cell->row < rows_n) {
		node_t *node_hor = (cell-1)->letter_hor->next;
		if (cell->col < cols_n) {
			return solve_grid_inter(node_hor, cell);
		}
		return solve_grid_final(node_hor->letters, cell+2);
	}
	if (cell->col < cols_n) {
		return solve_grid_final((cell-cols_total)->letter_ver->next->letters, cell+1);
	}
	if (!connected_whites || are_whites_connected(cell-cols_total-1)) {
		int i;
		blacks_ratio = (double)(blacks_n1-1)/cells_n;
		blacks_max = blacks_n1;
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
		return blacks_min >= blacks_max;
	}
	return 0;
}

static int solve_grid_inter(node_t *node_hor, cell_t *cell) {
	int hor_whites_min, hor_whites_max, ver_whites_min, ver_whites_max, choices_lo = choices_hi, choices_n, symmetric_bak, blacks_in_col, r, i, j;
	node_t *node_ver = (cell-cols_total)->letter_ver->next;
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
		if (blacks_n1+1 < blacks_max) {
			hor_whites_min = 0;
			ver_whites_min = 0;
		}
		else {
			hor_whites_min = hor_whites_max;
			ver_whites_min = ver_whites_max;
		}
	}
	i = 0;
	if (symmetric && cell->row > cell->col) {
		for (; i < node_hor->letters_n && node_hor->letters[i].symbol < cell->sym90->symbol; ++i);
	}
	if (node_hor != node_ver) {
		for (j = 0; i < node_hor->letters_n; ++i) {
			for (; j < node_ver->letters_n && node_ver->letters[j].symbol < node_hor->letters[i].symbol; ++j);
			if (j == node_ver->letters_n) {
				break;
			}
			if (node_ver->letters[j].symbol == node_hor->letters[i].symbol) {
				if (check_letter1(node_hor->letters+i, hor_whites_max, hor_whites_min) && check_letter1(node_ver->letters+j, ver_whites_max, ver_whites_min) && !add_choice(cell->symbol, node_hor->letters+i, node_ver->letters+j)) {
					return -1;
				}
				++j;
			}
		}
	}
	else {
		for (; i < node_hor->letters_n; ++i) {
			if (check_letter2(node_ver->letters+i, hor_whites_max, hor_whites_min, ver_whites_max, ver_whites_min) && !add_choice(cell->symbol, node_hor->letters+i, node_ver->letters+i)) {
				return -1;
			}
		}
	}
	if (sym_blacks) {
		if (cell->sym180 > cell) {
			unknown_cells_n -= 2;
		}
		else if (cell->sym180 == cell) {
			--unknown_cells_n;
		}
	}
	choices_n = choices_hi-choices_lo;
	if (choices_n > 1) {
		if (heuristic == HEURISTIC_FREQUENCY) {
			qsort(choices+choices_lo, (size_t)choices_n, sizeof(choice_t), compare_choices);
		}
		else if (heuristic == HEURISTIC_RANDOM) {
			for (i = choices_lo; i < choices_hi; ++i) {
				choice_t choice_tmp;
				j = (int)emtrand((unsigned long)(choices_hi-i))+i;
				choice_tmp = choices[i];
				choices[i] = choices[j];
				choices[j] = choice_tmp;
			}
		}
	}
	symmetric_bak = symmetric;
	blacks_in_col = cell->blacks_in_col;
	r = 0;
	for (i = choices_lo, j = 0; i < choices_hi && j < choices_max && !r; ++i) {
		copy_choice(cell, choices+i);
		if (symmetric_bak && cell->row > cell->col) {
			symmetric = cell->letter_hor->symbol == cell->sym90->symbol;
		}
		if (cell->letter_hor->symbol != SYMBOL_BLACK) {
			cell->blacks_in_col = cell->row+cell->letter_ver->len_max < rows_n ? 1+(cell+cell->letter_ver->len_max*cols_total)->blacks_in_col:0;
			blacks_n2 += cell->blacks_in_col-(cell-cols_total)->blacks_in_col;
			if (blacks_n1+blacks_n2 < blacks_max && (!sym_blacks || blacks_n2 <= blacks_n3+unknown_cells_n)) {
				roll_cell(cell, cell->letter_hor->symbol);
				if (connected_whites && cell->pos == blacks_n1+1) {
					first_white = cell;
				}
				r = solve_grid(cell+1);
				++j;
				unroll_cell(cell);
			}
			blacks_n2 -= cell->blacks_in_col-(cell-cols_total)->blacks_in_col;
		}
		else {
			++blacks_n1;
			blacks_n2 += cell->blacks_in_col-(cell-cols_total)->blacks_in_col;
			if (sym_blacks) {
				if (cell->sym180 > cell) {
					++blacks_n3;
				}
				else if (cell->sym180 < cell) {
					--blacks_n3;
				}
			}
			if (blacks_n1+blacks_n2 < blacks_max && (!sym_blacks || (blacks_n1+blacks_n3 < blacks_max && blacks_n2 <= blacks_n3+unknown_cells_n)) && (!linear_blacks || (double)blacks_n1/cell->pos <= blacks_ratio)) {
				roll_cell(cell, SYMBOL_BLACK);
				if (!connected_whites || are_whites_connected(cell)) {
					r = solve_grid(cell+1);
					++j;
				}
				unroll_cell(cell);
			}
			if (sym_blacks) {
				if (cell->sym180 > cell) {
					--blacks_n3;
				}
				else if (cell->sym180 < cell) {
					++blacks_n3;
				}
			}
			blacks_n2 -= cell->blacks_in_col-(cell-cols_total)->blacks_in_col;
			--blacks_n1;
		}
	}
	cell->blacks_in_col = blacks_in_col;
	symmetric = symmetric_bak;
	overflow |= choices_n > choices_max;
	choices_hi = choices_lo;
	if (sym_blacks) {
		if (cell->sym180 > cell) {
			unknown_cells_n += 2;
		}
		else if (cell->sym180 == cell) {
			++unknown_cells_n;
		}
	}
	return r;
}

static int check_letter1(letter_t *letter, int whites_max, int whites_min) {
	return letter->leaves_n && letter->len_min <= whites_max && letter->len_max >= whites_min;
}

static int check_letter2(letter_t *letter, int hor_whites_max, int hor_whites_min, int ver_whites_max, int ver_whites_min) {
	return letter->leaves_n > 1 && letter->len_min <= hor_whites_max && letter->len_max >= hor_whites_min && letter->len_min <= ver_whites_max && letter->len_max >= ver_whites_min;
}

static int add_choice(int cell_symbol, letter_t *letter_hor, letter_t *letter_ver) {
	if (letter_hor->symbol != SYMBOL_BLACK) {
		if (cell_symbol == SYMBOL_BLACK) {
			return 1;
		}
	}
	else {
		if (isupper(cell_symbol)) {
			return 1;
		}
	}
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
		hilo_multiply(letter_hor->leaves_n, letter_ver->leaves_n, &choice->leaves_lo, &choice->leaves_hi);
	}
}

static void hilo_multiply(int a, int b, int *lo, int *hi) {
	int a1, a2, b1, b2, m12, m21, carry1, carry2;
	if (a <= INT_MAX/b) {
		*lo = a*b;
		*hi = 0;
		return;
	}
	a1 = a & short_max;
	a2 = a >> short_bits;
	b1 = b & short_max;
	b2 = b >> short_bits;
	m12 = a1*b2;
	m21 = a2*b1;
	hilo_add(a1*b1, (m12 & short_max) << short_bits, lo, &carry1);
	hilo_add(*lo, (m21 & short_max) << short_bits, lo, &carry2);
	*hi = carry1+carry2+(m12 >> short_bits)+(m21 >> short_bits)+a2*b2;
}

static void hilo_add(int a, int b, int *lo, int *hi) {
	int delta = INT_MAX-b;
	if (a <= delta) {
		*lo = a+b;
		*hi = 0;
		return;
	}
	*lo = a-delta-1;
	*hi = 1;
}

static int compare_choices(const void *a, const void *b) {
	const choice_t *choice_a = (const choice_t *)a, *choice_b = (const choice_t *)b;
	if (choice_a->leaves_hi != choice_b->leaves_hi) {
		return choice_b->leaves_hi-choice_a->leaves_hi;
	}
	if (choice_a->leaves_lo != choice_b->leaves_lo) {
		return choice_b->leaves_lo-choice_a->leaves_lo;
	}
	return choice_a->letter_hor->symbol-choice_b->letter_hor->symbol;
}

static void copy_choice(cell_t *cell, choice_t *choice) {
	cell->letter_hor = choice->letter_hor;
	cell->letter_ver = choice->letter_ver;
}

static void roll_cell(cell_t *cell, int symbol) {
	--cell->letter_hor->leaves_n;
	--cell->letter_ver->leaves_n;
	cell->symbol = symbol;
	if (sym_blacks && cell->sym180 > cell) {
		cell->sym180->symbol = symbol;
	}
}

static void unroll_cell(cell_t *cell) {
	if (sym_blacks && cell->sym180 > cell) {
		cell->sym180->symbol = SYMBOL_UNKNOWN;
	}
	if (!sym_blacks || cell->sym180 >= cell) {
		cell->symbol = SYMBOL_UNKNOWN;
	}
	++cell->letter_ver->leaves_n;
	++cell->letter_hor->leaves_n;
}

static int solve_grid_final(letter_t *letter, cell_t *cell) {
	if (letter->symbol == SYMBOL_BLACK && letter->leaves_n) {
		int r;
		--letter->leaves_n;
		r = solve_grid(cell);
		++letter->leaves_n;
		return r;
	}
	return 0;
}

static int are_whites_connected(cell_t *cell) {
	int whites_n, i;
	if (cell->pos == blacks_n1 || (sym_blacks && blacks_n1 > blacks_n3+2)) {
		return 1;
	}
	whites_n = sym_blacks ? cells_n-unknown_cells_n-blacks_n1-blacks_n3:cell->pos-blacks_n1;
	queued_cells_n = 0;
	add_cell_to_queue(first_white);
	for (i = 0; i < queued_cells_n; ++i) {
		if (queued_cells[i]->symbol != SYMBOL_UNKNOWN) {
			--whites_n;
			if (!whites_n) {
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
	return !whites_n;
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
