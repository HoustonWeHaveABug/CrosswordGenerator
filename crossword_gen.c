#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>
#include "mtrand.h"

typedef struct node_s node_t;

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

typedef struct {
	int row;
	int col;
	letter_t *letter_hor;
	letter_t *letter_ver;
	int symbol;
	int visited;
}
cell_t;

typedef struct {
	letter_t *letter_hor;
	letter_t *letter_ver;
	int leaves_lo;
	int leaves_hi;
	int lens_sum;
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
static void set_cell(cell_t *, int, int, letter_t *, letter_t *, int);
static int solve_grid(cell_t *);
static int check_letter(node_t *, node_t *, letter_t *, int, int);
static int are_whites_connected(cell_t *, cell_t *, int, int);
static void add_cell_to_queue(cell_t *);
static int add_choice(letter_t *, letter_t *);
static void set_choice(choice_t *, letter_t *, letter_t *);
static void hilo_multiply(int, int, int *, int *);
static void hilo_add(int, int, int *, int *);
static int compare_choices(const void *, const void *);
static void copy_choice(cell_t *, choice_t *);
static int solve_grid_final(node_t *, cell_t *);
static void free_node(node_t *);

static int short_bits, short_max, rows_n, cols_n, blacks_min, blacks_max, choices_max, cols_total, choices_size, *blacks_in_cols, cells_n, sym_blacks, linear_blacks, connected_whites, heuristic, unknown_cells_n, choices_hi, symmetric, blacks_n1, blacks_n2, blacks_n3, whites_n, overflow, queued_cells_n;
static double blacks_ratio;
static letter_t letter_root;
static node_t *node_root;
static cell_t *cells, **queued_cells;
static choice_t *choices;

int main(int argc, char *argv[]) {
	int cells_max, options, r, j, i;
	FILE *fd;
	time_t mtseed;
	short_bits = (int)sizeof(int)*4;
	cells_max = 1 << short_bits;
	short_max = cells_max-1;
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <dictionary>\n", argv[0]);
		expected_parameters(cells_max);
		return EXIT_FAILURE;
	}
	if (scanf("%d%d%d%d%d%d", &rows_n, &cols_n, &blacks_min, &blacks_max, &options, &choices_max) != 6 || rows_n < 1 || rows_n > cols_n || rows_n > cells_max/cols_n || blacks_min < 0 || blacks_min > blacks_max || blacks_max > rows_n*cols_n || choices_max < 1) {
		fputs("Invalid grid settings\n", stderr);
		expected_parameters(cells_max);
		return EXIT_FAILURE;
	}
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
	letter_root.symbol = '#';
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
	set_cell(cells, -1, -1, NULL, NULL, '#');
	for (j = 1; j <= cols_n; ++j) {
		set_cell(cells+j, -1, j-1, NULL, &letter_root, '#');
	}
	set_cell(cells+j, -1, cols_n, NULL, NULL, '#');
	for (i = 1; i <= rows_n; ++i) {
		set_cell(cells+i*cols_total, i-1, -1, &letter_root, NULL, '#');
		for (j = 1; j <= cols_n; ++j) {
			set_cell(cells+i*cols_total+j, i-1, j-1, NULL, NULL, '.');
		}
		set_cell(cells+i*cols_total+j, i-1, cols_n, NULL, NULL, '#');
	}
	set_cell(cells+i*cols_total, rows_n, -1, NULL, NULL, '#');
	for (j = 1; j <= cols_n; ++j) {
		set_cell(cells+i*cols_total+j, rows_n, j-1, NULL, NULL, '#');
	}
	set_cell(cells+i*cols_total+j, rows_n, cols_n, NULL, NULL, '#');
	choices = malloc(sizeof(choice_t));
	if (!choices) {
		fputs("Could not allocate memory for choices\n", stderr);
		fflush(stderr);
		free(cells);
		free_node(node_root);
		return EXIT_FAILURE;
	}
	choices_size = 1;
	blacks_in_cols = calloc((size_t)cols_n, sizeof(int));
	if (!blacks_in_cols) {
		fputs("Could not allocate memory for blacks_in_cols\n", stderr);
		fflush(stderr);
		free(choices);
		free(cells);
		free_node(node_root);
		return EXIT_FAILURE;
	}
	cells_n = rows_n*cols_n;
	queued_cells = malloc(sizeof(cell_t *)*(size_t)cells_n);
	if (!queued_cells) {
		fputs("Could not allocate memory for queued_cells\n", stderr);
		fflush(stderr);
		free(blacks_in_cols);
		free(choices);
		free(cells);
		free_node(node_root);
		return EXIT_FAILURE;
	}
	sym_blacks = options & 1;
	linear_blacks = options & 2;
	connected_whites = options & 4;
	heuristic = options & 8;
	unknown_cells_n = cells_n;
	choices_hi = 0;
	symmetric = rows_n == cols_n;
	blacks_n1 = 0;
	blacks_n2 = 0;
	blacks_n3 = 0;
	blacks_ratio = (double)blacks_max/cells_n;
	whites_n = 0;
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
	free(blacks_in_cols);
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
	fputs("- Options (= sum of the below flags)\n", stderr);
	fputs("   - Black squares symmetry (0: disabled, 1: enabled)\n", stderr);
	fputs("   - Linear blacks (0: disabled, 2: enabled)\n", stderr);
	fputs("   - White squares connected (0: disabled, 4: enabled)\n", stderr);
	fputs("   - Heuristic (0: disabled, 8: enabled)\n", stderr);
	fputs("- Maximum number of choices at each step (> 0)\n", stderr);
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
			if (((blacks_max > 0 && len <= cols_n) || len == rows_n || len == cols_n) && !new_word(symbols, len)) {
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
	if (len > 0) {
		fputs("Unexpected end of dictionary\n", stderr);
		fflush(stderr);
		return 0;
	}
	return blacks_max == 0 || get_letter(node_root, '#') || new_letter(node_root, '#');
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
	return get_letter(node, '#') || new_letter(node, '#');
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
	if (node->letters_n > 0) {
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
	if (symbol != '#') {
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
	if (node->letters_n > 0) {
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
		child->leaves_n = letter != &letter_root ? 1:2;
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
	const letter_t *letter_a = (const letter_t *)a, *letter_b = (const letter_t *)b;
	return letter_a->symbol-letter_b->symbol;
}

static void set_cell(cell_t *cell, int row, int col, letter_t *letter_hor, letter_t *letter_ver, int symbol) {
	cell->row = row;
	cell->col = col;
	cell->letter_hor = letter_hor;
	cell->letter_ver = letter_ver;
	cell->symbol = symbol;
	cell->visited = 0;
}

static int solve_grid(cell_t *cell) {
	if (!cell->visited || (cell->row == rows_n && cell->col == cols_n)) {
		int i;
		cell->visited = 1;
		printf("CELL %d %d\n", cell->row, cell->col);
		printf("BLACK SQUARES %d", blacks_n1);
		if (blacks_n2 > 0) {
			printf("+%d", blacks_n2);
			if (blacks_n3 > 0) {
				printf("/%d", blacks_n3);
			}
		}
		else {
			if (blacks_n3 > 0) {
				printf("+0/%d", blacks_n3);
			}
		}
		puts("");
		for (i = 1; i <= rows_n; ++i) {
			int j;
			putchar(cells[i*cols_total+1].symbol);
			for (j = 2; j <= cols_n; ++j) {
				printf(" %c", cells[i*cols_total+j].symbol);
			}
			puts("");
		}
		fflush(stdout);
	}
	if (cell->row < rows_n) {
		node_t *node_hor = (cell-1)->letter_hor->next;
		if (cell->col < cols_n) {
			int ver_whites_min, ver_whites_max, hor_whites_min, hor_whites_max, symbol_sym90 = cells[(cell->col+1)*cols_total+cell->row+1].symbol, choices_lo = choices_hi, choices_n, symmetric_bak, blacks_in_col, r_white, r, i, j;
			node_t *node_ver = (cell-cols_total)->letter_ver->next;
			cell_t *cell_sym180 = cells+(rows_n-cell->row)*cols_total+cols_n-cell->col;
			if (sym_blacks) {
				cell_t *cell_sym;
				for (cell_sym = cell_sym180; cell_sym->row >= 0 && cell_sym->symbol != '.' && cell_sym->symbol != '#'; cell_sym -= cols_total);
				ver_whites_min = cell_sym180->row-cell_sym->row;
				for (; cell_sym->row >= 0 && cell_sym->symbol != '#'; cell_sym -= cols_total);
				ver_whites_max = cell_sym180->row-cell_sym->row;
				for (cell_sym = cell_sym180; cell_sym->col >= 0 && cell_sym->symbol != '.' && cell_sym->symbol != '#'; --cell_sym);
				hor_whites_min = cell_sym180->col-cell_sym->col;
				for (; cell_sym->col >= 0 && cell_sym->symbol != '#'; --cell_sym);
				hor_whites_max = cell_sym180->col-cell_sym->col;
				if (cell_sym180 > cell) {
					unknown_cells_n -= 2;
				}
				else if (cell_sym180 == cell) {
					--unknown_cells_n;
				}
			}
			else {
				ver_whites_max = rows_n-cell->row;
				hor_whites_max = cols_n-cell->col;
				if (blacks_n1+1 < blacks_max) {
					ver_whites_min = 0;
					hor_whites_min = 0;
				}
				else {
					ver_whites_min = ver_whites_max;
					hor_whites_min = hor_whites_max;
				}
			}
			i = 0;
			if (symmetric && cell->row > cell->col) {
				for (; i < node_hor->letters_n && node_hor->letters[i].symbol < symbol_sym90; ++i);
			}
			for (j = 0; i < node_hor->letters_n; ++i) {
				if (!check_letter(node_hor, node_ver, node_hor->letters+i, hor_whites_max, hor_whites_min)) {
					continue;
				}
				for (; j < node_ver->letters_n && node_ver->letters[j].symbol < node_hor->letters[i].symbol; ++j);
				if (j < node_ver->letters_n && node_ver->letters[j].symbol == node_hor->letters[i].symbol) {
					if (check_letter(node_hor, node_ver, node_ver->letters+j, ver_whites_max, ver_whites_min)) {
						if (node_ver->letters[j].symbol != '#') {
							if ((cell->symbol == '.' || cell->symbol == '*') && !add_choice(node_hor->letters+i, node_ver->letters+j)) {
								return -1;
							}
						}
						else {
							if ((cell->symbol == '.' || cell->symbol == '#') && !add_choice(node_hor->letters+i, node_ver->letters+j)) {
								return -1;
							}
						}
					}
					++j;
				}
			}
			choices_n = choices_hi-choices_lo;
			if (choices_n > 1) {
				if (heuristic) {
					qsort(choices+choices_lo, (size_t)choices_n, sizeof(choice_t), compare_choices);
				}
				else {
					for (i = choices_lo; i < choices_hi; ++i) {
						choice_t choice_tmp;
						j = (int)emtrand((unsigned long)(choices_hi-i))+i;
						choice_tmp = choices[i];
						choices[i] = choices[j];
						choices[j] = choice_tmp;
					}
				}
				if (choices_n > choices_max) {
					choices_hi = choices_lo+choices_max;
					if (!overflow) {
						overflow = 1;
					}
				}
			}
			symmetric_bak = symmetric;
			blacks_in_col = blacks_in_cols[cell->col];
			r_white = connected_whites ? -1:1;
			r = 0;
			for (i = choices_lo; i < choices_hi && !r; ++i) {
				copy_choice(cell, choices+i);
				if (symmetric_bak && cell->row > cell->col) {
					symmetric = cell->letter_hor->symbol == symbol_sym90;
				}
				if (cell->letter_hor->symbol != '#') {
					blacks_in_cols[cell->col] = cell->row+cell->letter_ver->len_max < rows_n ? 1+(rows_n-cell->row-cell->letter_ver->len_max-1)/(letter_root.len_max+1):0;
					blacks_n2 += blacks_in_cols[cell->col]-blacks_in_col;
					if (blacks_n1+blacks_n2 < blacks_max && (!sym_blacks || blacks_n2 <= blacks_n3+unknown_cells_n)) {
						if (connected_whites) {
							whites_n += sym_blacks && cell_sym180 > cell ? 2:1;
							if (r_white == -1) {
								r_white = are_whites_connected(cell, cell_sym180, whites_n, '*');
							}
						}
						if (r_white) {
							--cell->letter_hor->leaves_n;
							--cell->letter_ver->leaves_n;
							cell->symbol = cell->letter_hor->symbol;
							if (sym_blacks && cell_sym180 > cell) {
								cell_sym180->symbol = '*';
							}
							r = solve_grid(cell+1);
							if (sym_blacks && cell_sym180 > cell) {
								cell_sym180->symbol = '.';
							}
							cell->symbol = sym_blacks && cell_sym180 < cell ? '*':'.';
							++cell->letter_ver->leaves_n;
							++cell->letter_hor->leaves_n;
						}
						if (connected_whites) {
							whites_n -= sym_blacks && cell_sym180 > cell ? 2:1;
						}
					}
					blacks_n2 -= blacks_in_cols[cell->col]-blacks_in_col;
				}
				else {
					++blacks_n1;
					blacks_in_cols[cell->col] = (rows_n-cell->row-1)/(letter_root.len_max+1);
					blacks_n2 += blacks_in_cols[cell->col]-blacks_in_col;
					if (sym_blacks) {
						if (cell_sym180 > cell) {
							++blacks_n3;
						}
						else if (cell_sym180 < cell) {
							--blacks_n3;
						}
					}
					if (blacks_n1+blacks_n2 < blacks_max && (!sym_blacks || (blacks_n1+blacks_n3 < blacks_max && blacks_n2 <= blacks_n3+unknown_cells_n)) && (!linear_blacks || (cell->row == 0 && cell->col == 0) || (double)blacks_n1/(cell->row*cols_n+cell->col) <= blacks_ratio) && (!connected_whites || are_whites_connected(cell, cell_sym180, whites_n, '#'))) {
						if (node_hor != node_root) {
							--cell->letter_hor->leaves_n;
						}
						if (node_ver != node_root) {
							--cell->letter_ver->leaves_n;
						}
						if (!sym_blacks || cell_sym180 >= cell) {
							cell->symbol = '#';
						}
						if (sym_blacks && cell_sym180 > cell) {
							cell_sym180->symbol = '#';
						}
						r = solve_grid(cell+1);
						if (sym_blacks && cell_sym180 > cell) {
							cell_sym180->symbol = '.';
						}
						if (!sym_blacks || cell_sym180 >= cell) {
							cell->symbol = '.';
						}
						if (node_ver != node_root) {
							++cell->letter_ver->leaves_n;
						}
						if (node_hor != node_root) {
							++cell->letter_hor->leaves_n;
						}
					}
					if (sym_blacks) {
						if (cell_sym180 > cell) {
							--blacks_n3;
						}
						else if (cell_sym180 < cell) {
							++blacks_n3;
						}
					}
					blacks_n2 -= blacks_in_cols[cell->col]-blacks_in_col;
					--blacks_n1;
				}
			}
			blacks_in_cols[cell->col] = blacks_in_col;
			symmetric = symmetric_bak;
			choices_hi = choices_lo;
			if (sym_blacks) {
				if (cell_sym180 > cell) {
					unknown_cells_n += 2;
				}
				else if (cell_sym180 == cell) {
					++unknown_cells_n;
				}
			}
			return r;
		}
		return solve_grid_final(node_hor, cell+2);
	}
	if (cell->col < cols_n) {
		return solve_grid_final((cell-cols_total)->letter_ver->next, cell+1);
	}
	blacks_max = blacks_n1;
	puts("SOLUTION FOUND");
	fflush(stdout);
	return blacks_min >= blacks_max;
}

static int check_letter(node_t *node_hor, node_t *node_ver, letter_t *letter, int whites_max, int whites_min) {
	return ((node_hor != node_ver && letter->leaves_n > 0) || letter->leaves_n > 1) && letter->len_min <= whites_max && letter->len_max >= whites_min;
}

static int are_whites_connected(cell_t *cell, cell_t *cell_sym180, int target, int symbol) {
	int i;
	cell_t *cell_first;
	if (sym_blacks && cell_sym180 < cell) {
		return 1;
	}
	if (symbol == '*') {
		cell_first = cell;
	}
	else {
		cell_first = NULL;
		for (i = 1; i <= rows_n; ++i) {
			int j;
			for (j = 1; j <= cols_n; ++j) {
				if (cells[i*cols_total+j].symbol != '#') {
					cell_first = cells+i*cols_total+j;
					break;
				}
			}
			if (j <= cols_n) {
				break;
			}
		}
		if (!cell_first) {
			return 1;
		}
	}
	cell->symbol = symbol;
	if (sym_blacks && cell_sym180 > cell) {
		cell_sym180->symbol = symbol;
	}
	queued_cells_n = 0;
	add_cell_to_queue(cell_first);
	for (i = 0; i < queued_cells_n; ++i) {
		if (queued_cells[i]->symbol != '.') {
			--target;
		}
		if (target == 0) {
			break;
		}
		add_cell_to_queue(queued_cells[i]-1);
		add_cell_to_queue(queued_cells[i]-cols_total);
		add_cell_to_queue(queued_cells[i]+1);
		add_cell_to_queue(queued_cells[i]+cols_total);
	}
	for (i = queued_cells_n; i--; ) {
		queued_cells[i]->visited ^= 2;
	}
	if (sym_blacks && cell_sym180 > cell) {
		cell_sym180->symbol = '.';
	}
	cell->symbol = '.';
	return target == 0;
}

static void add_cell_to_queue(cell_t *cell) {
	if (cell->symbol != '#' && !(cell->visited & 2)) {
		cell->visited |= 2;
		queued_cells[queued_cells_n++] = cell;
	}
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
	if (heuristic) {
		hilo_multiply(choice->letter_hor->leaves_n, choice->letter_ver->leaves_n, &choice->leaves_lo, &choice->leaves_hi);
	}
	choice->lens_sum = letter_hor->len_min+letter_ver->len_min+letter_hor->len_max+letter_ver->len_max;
}

static void hilo_multiply(int a, int b, int *lo, int *hi) {
	int a1 = a & short_max, a2 = a >> short_bits, b1 = b & short_max, b2 = b >> short_bits, m12 = a1*b2, m21 = a2*b1, carry1, carry2;
	hilo_add(a1*b1, (m12 & short_max) << short_bits, lo, &carry1);
	hilo_add(*lo, (m21 & short_max) << short_bits, lo, &carry2);
	*hi = carry1+carry2+(m12 >> short_bits)+(m21 >> short_bits)+a2*b2;
}

static void hilo_add(int a, int b, int *lo, int *hi) {
	int delta = INT_MAX-b;
	if (a > delta) {
		*lo = a-delta-1;
		*hi = 1;
	}
	else {
		*lo = a+b;
		*hi = 0;
	}
}

static int compare_choices(const void *a, const void *b) {
	const choice_t *choice_a = (const choice_t *)a, *choice_b = (const choice_t *)b;
	if (choice_a->leaves_hi != choice_b->leaves_hi) {
		return choice_b->leaves_hi-choice_a->leaves_hi;
	}
	if (choice_a->leaves_lo != choice_b->leaves_lo) {
		return choice_b->leaves_lo-choice_a->leaves_lo;
	}
	if (choice_a->lens_sum != choice_b->lens_sum) {
		return choice_b->lens_sum-choice_a->lens_sum;
	}
	return choice_b->letter_hor->symbol-choice_a->letter_hor->symbol;
}

static void copy_choice(cell_t *cell, choice_t *choice) {
	cell->letter_hor = choice->letter_hor;
	cell->letter_ver = choice->letter_ver;
}

static int solve_grid_final(node_t *node, cell_t *cell) {
	if (node->letters[0].symbol == '#' && node->letters[0].leaves_n > 0) {
		int r;
		if (node != node_root) {
			--node->letters[0].leaves_n;
		}
		r = solve_grid(cell);
		if (node != node_root) {
			++node->letters[0].leaves_n;
		}
		return r;
	}
	return 0;
}

static void free_node(node_t *node) {
	if (node->letters_n > 0) {
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
