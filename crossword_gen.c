#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include "mtrand.h"

typedef struct node_s node_t;

typedef struct {
	int symbol;
	int type;
	int used;
	node_t *next;
	int leafs_n;
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
	node_t *node_hor;
	node_t *node_ver;
	int symbol;
	int visited;
}
cell_t;

typedef struct {
	letter_t *letter_hor;
	letter_t *letter_ver;
}
choice_t;

int load_dictionary(FILE *);
int new_word(int *, int, int);
letter_t *get_letter(node_t *, int, int);
letter_t *new_letter(node_t *, int, int);
node_t *new_node(void);
void sort_node(letter_t *, node_t *);
int compare_letters(const void *, const void *);
int set_cells(void);
void set_cell(cell_t *, int, int, node_t *, node_t *, int);
int set_choices(void);
int set_black_in_cols(void);
int solve_grid(cell_t *, int, int, int, int, int);
int add_choice(letter_t *, letter_t *, int *);
void set_choice(choice_t *, letter_t *, letter_t *);
int compare_choices(const void *, const void *);
void copy_choice(cell_t *, choice_t *);
int solve_grid_final(node_t *, cell_t *, int, int, int, int, int);
void print_grid(int, int);
void free_node(node_t *);

int rows_n, cols_n, blacks_max, sym_blacks, linear_blacks, heuristic, choices_max, cols_total, choices_size, *black_in_cols, overflow;
node_t *root;
cell_t *cells;
choice_t *choices;

int main(int argc, char *argv[]) {
	int r;
	FILE *fd;
	letter_t letter;
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <dictionary>\n", argv[0]);
		fprintf(stderr, "Parameters expected on the standard input:\n");
		fprintf(stderr, "- Number of rows (> 0)\n");
		fprintf(stderr, "- Number of columns (>= Number of rows)\n");
		fprintf(stderr, "- Maximum number of black squares (>= 0)\n");
		fprintf(stderr, "- Black squares symmetry flag (0: false, otherwise true)\n");
		fprintf(stderr, "- Linear blacks flag (0: false, otherwise true)\n");
		fprintf(stderr, "- Heuristic flag (0: false, otherwise true)\n");
		fprintf(stderr, "- Maximum number of choices at each step (> 0)\n");
		fflush(stderr);
		return EXIT_FAILURE;
	}
	if (scanf("%d%d%d%d%d%d%d", &rows_n, &cols_n, &blacks_max, &sym_blacks, &linear_blacks, &heuristic, &choices_max) != 7 || rows_n < 1 || rows_n > cols_n || blacks_max < 0 || blacks_max > rows_n*cols_n || choices_max < 1) {
		fprintf(stderr, "Invalid grid settings\n");
		fflush(stderr);
		return EXIT_FAILURE;
	}
	cols_total = cols_n+2;
	fd = fopen(argv[1], "r");
	if (!fd) {
		fprintf(stderr, "Could not open dictionary\n");
		fflush(stderr);
		return EXIT_FAILURE;
	}
	root = new_node();
	if (!root) {
		fclose(fd);
		return 0;
	}
	if (!load_dictionary(fd)) {
		free_node(root);
		fclose(fd);
		return EXIT_FAILURE;
	}
	sort_node(&letter, root);
	if (!set_cells()) {
		free_node(root);
		fclose(fd);
		return EXIT_FAILURE;
	}
	if (!set_choices()) {
		free(cells);
		free_node(root);
		fclose(fd);
		return EXIT_FAILURE;
	}
	if (!set_black_in_cols()) {
		free(choices);
		free(cells);
		free_node(root);
		fclose(fd);
		return EXIT_FAILURE;
	}
	smtrand((unsigned long)time(NULL));
	blacks_max++;
	do {
		printf("CHOICES %d\n", choices_max);
		fflush(stdout);
		overflow = 0;
		r = solve_grid(cells+cols_total+1, 0, 0, 0, 0, rows_n == cols_n);
		if (overflow) {
			choices_max++;
		}
	}
	while (overflow && !r);
	free(black_in_cols);
	free(choices);
	free(cells);
	free_node(root);
	fclose(fd);
	return EXIT_SUCCESS;
}

int load_dictionary(FILE *fd) {
	int *symbols = malloc(sizeof(int)*(size_t)cols_n), line, len, c;
	if (!symbols) {
		fprintf(stderr, "Could not allocate memory for symbols\n");
		fflush(stderr);
		return 0;
	}
	line = 0;
	len = 0;
	c = fgetc(fd);
	while (!feof(fd)) {
		line++;
		if (islower(c)) {
			c = toupper(c);
		}
		if (isupper(c)) {
			if (len < cols_n) {
				symbols[len] = c;
			}
			len++;
		}
		else if (c == '\n') {
			if (((blacks_max > 0 && len <= rows_n) || len == rows_n) && !new_word(symbols, len, 0)) {
				free(symbols);
				return 0;
			}
			if (rows_n < cols_n && ((blacks_max > 0 && len > rows_n && len <= cols_n) || len == cols_n) && !new_word(symbols, len, 1)) {
				free(symbols);
				return 0;
			}
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
	if (len > 0) {
		fprintf(stderr, "Unexpected end of dictionary\n");
		fflush(stderr);
		free(symbols);
		return 0;
	}
	if (blacks_max > 0 && !get_letter(root, '#', 0) && !new_letter(root, '#', 0)) {
		free(symbols);
		return 0;
	}
	free(symbols);
	return 1;
}

int new_word(int *symbols, int len, int type) {
	int i;
	node_t *node = root;
	for (i = 0; i < len; i++) {
		letter_t *letter = get_letter(node, symbols[i], type);
		if (!letter) {
			letter = new_letter(node, symbols[i], type);
			if (!letter) {
				return 0;
			}
		}
		node = letter->next;
	}
	return get_letter(node, '#', type) || new_letter(node, '#', type);
}

letter_t *get_letter(node_t *node, int symbol, int type) {
	int i;
	for (i = 0; i < node->letters_n && (node->letters[i].symbol != symbol || node->letters[i].type != type); i++);
	if (i < node->letters_n) {
		return node->letters+i;
	}
	return NULL;
}

letter_t *new_letter(node_t *node, int symbol, int type) {
	letter_t *letter;
	if (node->letters) {
		letter_t *letters = realloc(node->letters, sizeof(letter_t)*(size_t)(node->letters_n+1));
		if (!letters) {
			fprintf(stderr, "Could not reallocate memory for node->letters\n");
			fflush(stderr);
			return NULL;
		}
		node->letters = letters;
	}
	else {
		node->letters = malloc(sizeof(letter_t));
		if (!node->letters) {
			fprintf(stderr, "Could not allocate memory for node->letters\n");
			fflush(stderr);
			return NULL;
		}
	}
	letter = node->letters+node->letters_n;
	letter->symbol = symbol;
	letter->type = type;
	letter->used = 0;
	if (symbol != '#') {
		letter->next = new_node();
		if (!letter->next) {
			return NULL;
		}
	}
	else {
		letter->next = root;
	}
	node->letters_n++;
	return letter;
}

node_t *new_node(void) {
	node_t *node = malloc(sizeof(node_t));
	if (!node) {
		fprintf(stderr, "Could not allocate memory for node\n");
		fflush(stderr);
		return NULL;
	}
	node->letters_n = 0;
	node->letters = NULL;
	return node;
}

void sort_node(letter_t *letter, node_t *node) {
	letter->leafs_n = 0;
	letter->len_min = cols_n;
	letter->len_max = 0;
	if (node->letters_n > 0) {
		int i;
		for (i = 0; i < node->letters_n; i++) {
			if (node->letters[i].next != root) {
				sort_node(node->letters+i, node->letters[i].next);
				node->letters[i].len_min++;
				node->letters[i].len_max++;
			}
			else {
				node->letters[i].leafs_n = 1;
				node->letters[i].len_min = 0;
				node->letters[i].len_max = 0;
			}
			letter->leafs_n += node->letters[i].leafs_n;
			if (node->letters[i].len_min < letter->len_min) {
				letter->len_min = node->letters[i].len_min;
			}
			if (node->letters[i].len_max > letter->len_max) {
				letter->len_max = node->letters[i].len_max;
			}
		}
		qsort(node->letters, (size_t)node->letters_n, sizeof(letter_t), compare_letters);
	}
}

int compare_letters(const void *a, const void *b) {
	const letter_t *letter_a = (const letter_t *)a, *letter_b = (const letter_t *)b;
	if (letter_a->symbol != letter_b->symbol) {
		return letter_a->symbol-letter_b->symbol;
	}
	return letter_a->type-letter_b->type;
}

int set_cells(void) {
	int j, i;
	cells = malloc(sizeof(cell_t)*(size_t)((rows_n+2)*cols_total));
	if (!cells) {
		fprintf(stderr, "Could not allocate memory for cells\n");
		fflush(stderr);
		return 0;
	}
	set_cell(cells, -1, -1, NULL, NULL, '.');
	for (j = 1; j <= cols_n; j++) {
		set_cell(cells+j, -1, j-1, NULL, root, '#');
	}
	set_cell(cells+j, -1, cols_n, NULL, NULL, '.');
	for (i = 1; i <= rows_n; i++) {
		set_cell(cells+i*cols_total, i-1, -1, root, NULL, '#');
		for (j = 1; j < cols_total; j++) {
			set_cell(cells+i*cols_total+j, i-1, j-1, NULL, NULL, '.');
		}
	}
	for (j = 0; j < cols_total; j++) {
		set_cell(cells+i*cols_total+j, rows_n, j-1, NULL, NULL, '.');
	}
	return 1;
}

void set_cell(cell_t *cell, int row, int col, node_t *node_hor, node_t *node_ver, int symbol) {
	cell->row = row;
	cell->col = col;
	cell->node_hor = node_hor;
	cell->node_ver = node_ver;
	cell->symbol = symbol;
	cell->visited = 0;
}

int set_choices(void) {
	choices = malloc(sizeof(choice_t));
	if (!choices) {
		fprintf(stderr, "Could not allocate memory for choices\n");
		fflush(stderr);
		return 0;
	}
	choices_size = 1;
	return 1;
}

int set_black_in_cols(void) {
	int i;
	black_in_cols = malloc(sizeof(int)*(size_t)cols_n);
	if (!black_in_cols) {
		fprintf(stderr, "Could not allocate memory for black_in_cols\n");
		fflush(stderr);
		return 0;
	}
	for (i = 0; i < cols_n; i++) {
		black_in_cols[i] = rows_n;
	}
	return 1;
}

int solve_grid(cell_t *cell, int choices_lo, int blacks_n1, int blacks_n2, int blacks_n3, int symmetric) {
	cell_t *cell_sym180 = cells+(rows_n-cell->row)*cols_total+cols_n-cell->col;
	if (!cell->visited) {
		cell->visited = 1;
		printf("CELL %d %d\n", cell->row, cell->col);
		print_grid(blacks_n1, blacks_n2);
	}
	if (blacks_n1+blacks_n2 >= blacks_max || blacks_n3 >= blacks_max || (cell > cell_sym180 && blacks_n1+blacks_n2 > blacks_n3) || (linear_blacks && (cell->row > 0 || cell->col > 0) && (double)blacks_n1/(cell->row*cols_n+cell->col) > (double)blacks_max/(rows_n*cols_n))) {
		return 0;
	}
	if (cell->row < rows_n) {
		node_t *node_hor = (cell-1)->node_hor;
		if (cell->col < cols_n) {
			int row_whites_max, row_whites_min, col_whites_max, col_whites_min, choices_hi = choices_lo, choices_n, r, i, j;
			node_t *node_ver = (cell-cols_total)->node_ver;
			cell_t *cell_sym90;
			if (!sym_blacks || cell <= cell_sym180) {
				row_whites_max = cols_n-cell->col;
				col_whites_max = rows_n-cell->row;
				if (blacks_n1+blacks_n2+1 < blacks_max) {
					row_whites_min = 0;
					col_whites_min = 0;
				}
				else {
					row_whites_min = row_whites_max;
					col_whites_min = col_whites_max;
				}
			}
			else {
				cell_t *row_start_sym = cell_sym180-cell_sym180->col, *col_start_sym = cell_sym180-cell_sym180->row*cols_total, *cell_sym;
				row_whites_max = 0;
				for (cell_sym = cell_sym180; cell_sym >= row_start_sym && cell_sym->symbol != '#'; cell_sym--) {
					row_whites_max++;
				}
				row_whites_min = row_whites_max;
				col_whites_max = 0;
				for (cell_sym = cell_sym180; cell_sym >= col_start_sym && cell_sym->symbol != '#'; cell_sym -= cols_total) {
					col_whites_max++;
				}
				col_whites_min = col_whites_max;
			}
			for (i = 0, j = 0; i < node_hor->letters_n; i++) {
				if (node_hor->letters[i].used >= node_hor->letters[i].leafs_n || node_hor->letters[i].len_min > row_whites_max || node_hor->letters[i].len_max < row_whites_min) {
					continue;
				}
				while (j < node_ver->letters_n && node_ver->letters[j].symbol < node_hor->letters[i].symbol) {
					j++;
				}
				if (j < node_ver->letters_n && node_ver->letters[j].symbol == node_hor->letters[i].symbol) {
					if (node_ver->letters[j].used < node_ver->letters[j].leafs_n && node_ver->letters[j].len_min <= col_whites_max && node_ver->letters[j].len_max >= col_whites_min && !add_choice(node_hor->letters+i, node_ver->letters+j, &choices_hi)) {
						return -1;
					}
					j++;
				}
			}
			choices_n = choices_hi-choices_lo;
			if (choices_n == 0) {
				return 0;
			}
			if (heuristic) {
				qsort(choices+choices_lo, (size_t)choices_n, sizeof(choice_t), compare_choices);
			}
			else {
				for (i = 0; i < choices_n; i++) {
					choice_t choice_tmp;
					j = (int)emtrand((unsigned long)(choices_n-i))+i+choices_lo;
					choice_tmp = choices[i+choices_lo];
					choices[i+choices_lo] = choices[j];
					choices[j] = choice_tmp;
				}
			}
			if (choices_n > choices_max) {
				choices_hi = choices_lo+choices_max;
				if (!overflow) {
					overflow = 1;
				}
			}
			cell_sym90 = cells+(cell->col+1)*cols_total+cell->row+1;
			r = 0;
			for (i = choices_lo; i < choices_hi && !r; i++) {
				int symmetric_new;
				if (!symmetric || cell->row <= cell->col) {
					symmetric_new = symmetric;
				}
				else {
					if (choices[i].letter_hor->symbol < cell_sym90->symbol) {
						continue;
					}
					symmetric_new = choices[i].letter_hor->symbol == cell_sym90->symbol;
				}
				if (choices[i].letter_hor->symbol != '#') {
					choices[i].letter_hor->used++;
					if (choices[i].letter_ver->used < choices[i].letter_ver->leafs_n) {
						int blacks_n2_new;
						choices[i].letter_ver->used++;
						copy_choice(cell, choices+i);
						blacks_n2_new = blacks_n2;
						if (cell->row+choices[i].letter_ver->len_max < rows_n && black_in_cols[cell->col] == rows_n) {
							black_in_cols[cell->col] = cell->row;
							blacks_n2_new++;
						}
						r = solve_grid(cell+1, choices_hi, blacks_n1, blacks_n2_new, blacks_n3, symmetric_new);
						if (cell->row+choices[i].letter_ver->len_max < rows_n && black_in_cols[cell->col] == cell->row) {
							black_in_cols[cell->col] = rows_n;
						}
						cell->symbol = '.';
						choices[i].letter_ver->used--;
					}
					choices[i].letter_hor->used--;
				}
				else {
					if (node_hor != root) {
						choices[i].letter_hor->used++;
					}
					if (choices[i].letter_ver->used < choices[i].letter_ver->leafs_n) {
						int black_in_col, blacks_n2_new, blacks_n3_new;
						if (node_ver != root) {
							choices[i].letter_ver->used++;
						}
						copy_choice(cell, choices+i);
						black_in_col = black_in_cols[cell->col];
						blacks_n2_new = blacks_n2;
						if (cell->row+choices[i].letter_ver->len_max < rows_n && black_in_col < rows_n) {
							black_in_cols[cell->col] = rows_n;
							blacks_n2_new--;
						}
						if (sym_blacks) {
							blacks_n3_new = blacks_n3;
							if (cell < cell_sym180) {
								blacks_n3_new += 2;
							}
							else if (cell == cell_sym180) {
								blacks_n3_new++;
							}
						}
						else {
							blacks_n3_new = blacks_n1+blacks_n2;
						}
						r = solve_grid(cell+1, choices_hi, blacks_n1+1, blacks_n2_new, blacks_n3_new, symmetric_new);
						if (cell->row+choices[i].letter_ver->len_max < rows_n && black_in_col < rows_n) {
							black_in_cols[cell->col] = black_in_col;
						}
						cell->symbol = '.';
						if (node_ver != root) {
							choices[i].letter_ver->used--;
						}
					}
					if (node_hor != root) {
						choices[i].letter_hor->used--;
					}
				}
			}
			return r;
		}
		else {
			return solve_grid_final(node_hor, cell+2, choices_lo, blacks_n1, blacks_n2, blacks_n3, symmetric);
		}
	}
	else {
		if (cell->col < cols_n) {
			node_t *node_ver = (cell-cols_total)->node_ver;
			return solve_grid_final(node_ver, cell+1, choices_lo, blacks_n1, blacks_n2, blacks_n3, symmetric);
		}
		else {
			blacks_max = blacks_n1+blacks_n2;
			puts("SOLUTION");
			print_grid(blacks_n1, blacks_n2);
			return 1;
		}
	}
}

int add_choice(letter_t *letter_hor, letter_t *letter_ver, int *choices_hi) {
	if (*choices_hi == choices_size) {
		choice_t *choices_tmp = realloc(choices, sizeof(choice_t)*(size_t)(*choices_hi+1));
		if (!choices_tmp) {
			fprintf(stderr, "Could not reallocate memory for choices\n");
			fflush(stderr);
			return 0;
		}
		choices = choices_tmp;
		choices_size = *choices_hi+1;
	}
	set_choice(choices+*choices_hi, letter_hor, letter_ver);
	*choices_hi = *choices_hi+1;
	return 1;
}

void set_choice(choice_t *choice, letter_t *letter_hor, letter_t *letter_ver) {
	choice->letter_hor = letter_hor;
	choice->letter_ver = letter_ver;
}

int compare_choices(const void *a, const void *b) {
	int sum_a, sum_b;
	double factor_a, factor_b;
	const choice_t *choice_a = (const choice_t *)a, *choice_b = (const choice_t *)b;
	sum_a = choice_a->letter_hor->type+choice_a->letter_ver->type;
	sum_b = choice_b->letter_hor->type+choice_b->letter_ver->type;
	if (sum_a != sum_b) {
		return sum_b-sum_a;
	}
	factor_a = (double)(choice_a->letter_hor->leafs_n-choice_a->letter_hor->used)*(choice_a->letter_ver->leafs_n-choice_a->letter_ver->used);
	factor_b = (double)(choice_b->letter_hor->leafs_n-choice_b->letter_hor->used)*(choice_b->letter_ver->leafs_n-choice_b->letter_ver->used);
	if (factor_a < factor_b) {
		return 1;
	}
	if (factor_a > factor_b) {
		return -1;
	}
	sum_a = choice_a->letter_hor->len_min+choice_a->letter_ver->len_min+choice_a->letter_hor->len_max+choice_a->letter_ver->len_max;
	sum_b = choice_b->letter_hor->len_min+choice_b->letter_ver->len_min+choice_b->letter_hor->len_max+choice_b->letter_ver->len_max;
	if (sum_a != sum_b) {
		return sum_b-sum_a;
	}
	return choice_b->letter_hor->symbol-choice_a->letter_hor->symbol;
}

void copy_choice(cell_t *cell, choice_t *choice) {
	cell->node_hor = choice->letter_hor->next;
	cell->node_ver = choice->letter_ver->next;
	cell->symbol = choice->letter_hor->symbol;
}

int solve_grid_final(node_t *node, cell_t *cell, int choices_lo, int blacks_n1, int blacks_n2, int blacks_n3, int symmetric) {
	int i;
	for (i = 0; i < node->letters_n && node->letters[i].symbol == '#'; i++) {
		if (node->letters[i].used < node->letters[i].leafs_n) {
			int r;
			if (node != root) {
				node->letters[i].used++;
			}
			r = solve_grid(cell, choices_lo, blacks_n1, blacks_n2, blacks_n3, symmetric);
			if (node != root) {
				node->letters[i].used--;
			}
			return r;
		}
	}
	return 0;
}

void print_grid(int blacks_n1, int blacks_n2) {
	int i;
	printf("BLACK SQUARES %d", blacks_n1);
	if (blacks_n2 > 0) {
		printf("+%d", blacks_n2);
	}
	puts("");
	for (i = 1; i <= rows_n; i++) {
		int j;
		putchar(cells[i*cols_total+1].symbol);
		for (j = 2; j <= cols_n; j++) {
			printf(" %c", cells[i*cols_total+j].symbol);
		}
		puts("");
	}
	fflush(stdout);
}

void free_node(node_t *node) {
	if (node->letters_n > 0) {
		int i;
		for (i = 0; i < node->letters_n; i++) {
			if (node->letters[i].next != root) {
				free_node(node->letters[i].next);
			}
		}
		free(node->letters);
	}
	free(node);
}
