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
int compare_symbols(const void *, const void *);
int set_cells(void);
void set_cell(cell_t *, int, int, node_t *, node_t *, int);
int set_choices(void);
int set_black_in_cols(void);
cell_t *solve_grid(cell_t *, int, int, int, int);
cell_t *solve_line(cell_t *, int, int, int, int);
int check_blacks(cell_t *, int, int);
int add_choice(letter_t *, letter_t *, int *);
void set_choice(choice_t *, letter_t *, letter_t *);
int compare_choices(const void *, const void *);
int compare_letters(letter_t *, letter_t *);
int set_symmetric(cell_t *, int, cell_t *, letter_t *);
void copy_choice(cell_t *, choice_t *);
cell_t *solve_white(letter_t *, cell_t *, int, int, int, int, cell_t *(*)(cell_t *, int, int, int, int));
cell_t *solve_black(letter_t *, int, cell_t *, int, int, int, int, cell_t *(*)(cell_t *, int, int, int, int));
cell_t *solve_grid_final(node_t *, cell_t *, int, int, int, int);
void print_grid(int, int);
void free_node(node_t *);

int rows_n, cols_n, blacks_max, linear_blacks, heuristic, cols_offset, choices_max, *black_in_cols, col_max;
node_t *root;
cell_t *cells;
choice_t *choices;

int main(int argc, char *argv[]) {
	FILE *fd;
	letter_t letter;
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <dictionary>\n", argv[0]);
		fflush(stderr);
		return EXIT_FAILURE;
	}
	if (scanf("%d%d%d%d%d", &rows_n, &cols_n, &blacks_max, &linear_blacks, &heuristic) != 5 || rows_n < 1 || rows_n > cols_n || blacks_max < 0 || blacks_max > rows_n*cols_n) {
		fprintf(stderr, "Invalid grid settings\n");
		fflush(stderr);
		return EXIT_FAILURE;
	}
	cols_offset = cols_n+2;
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
	solve_grid(cells+cols_offset+1, 0, 0, 0, rows_n == cols_n);
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
		qsort(node->letters, (size_t)node->letters_n, sizeof(letter_t), compare_symbols);
	}
}

int compare_symbols(const void *a, const void *b) {
	const letter_t *letter_a = (const letter_t *)a, *letter_b = (const letter_t *)b;
	if (letter_a->symbol != letter_b->symbol) {
		return letter_a->symbol-letter_b->symbol;
	}
	return letter_a->type-letter_b->type;
}

int set_cells(void) {
	int j, i;
	cells = malloc(sizeof(cell_t)*(size_t)((rows_n+2)*cols_offset));
	if (!cells) {
		fprintf(stderr, "Could not allocate memory for cells\n");
		fflush(stderr);
		return 0;
	}
	set_cell(cells, -1, -1, NULL, NULL, '.');
	for (j = 1; j <= cols_n; j++) {
		set_cell(cells+j, -1, j-1, NULL, root, '#');
	}
	set_cell(cells+j, -1, j-1, NULL, NULL, '.');
	for (i = 1; i <= rows_n; i++) {
		set_cell(cells+i*cols_offset, i-1, -1, root, NULL, '#');
		for (j = 1; j < cols_offset; j++) {
			set_cell(cells+i*cols_offset+j, i-1, j-1, NULL, NULL, '.');
		}
	}
	for (j = 0; j < cols_offset; j++) {
		set_cell(cells+i*cols_offset+j, i-1, j-1, NULL, NULL, '.');
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
	choices_max = 1;
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

cell_t *solve_grid(cell_t *cell, int choices_lo, int blacks_n1, int blacks_n2, int symmetric) {
	if (!cell->visited) {
		cell->visited = 1;
		printf("CELL %d %d\n", cell->row, cell->col);
		print_grid(blacks_n1, blacks_n2);
	}
	if (cell->row > 0 && cell->col == 0) {
		col_max = -1;
		solve_line(cell, choices_lo, blacks_n1, blacks_n2, symmetric);
		if (col_max < cols_n+1) {
			return cell-cols_offset+col_max;
		}
	}
	if (!check_blacks(cell, blacks_n1, blacks_n2)) {
		return cell;
	}
	if (cell->row < rows_n) {
		node_t *node_hor = (cell-1)->node_hor;
		if (cell->col < cols_n) {
			int choices_hi = choices_lo, choices_n, i, j;
			node_t *node_ver = (cell-cols_offset)->node_ver;
			cell_t *cell_sym = cells+(cell->col+1)*cols_offset+cell->row+1, *r;
			for (i = 0, j = 0; i < node_hor->letters_n; i++) {
				if (node_hor->letters[i].used >= node_hor->letters[i].leafs_n || cell->col+node_hor->letters[i].len_min > cols_n || (cell->col+node_hor->letters[i].len_max < cols_n && blacks_n1+blacks_n2+1 >= blacks_max)) {
					continue;
				}
				while (j < node_ver->letters_n && node_ver->letters[j].symbol < node_hor->letters[i].symbol) {
					j++;
				}
				while (j < node_ver->letters_n && node_ver->letters[j].symbol == node_hor->letters[i].symbol) {
					if (node_ver->letters[j].used < node_ver->letters[j].leafs_n && cell->row+node_ver->letters[j].len_min <= rows_n && (cell->row+node_ver->letters[j].len_max >= rows_n || blacks_n1+blacks_n2+1 < blacks_max) && !add_choice(node_hor->letters+i, node_ver->letters+j, &choices_hi)) {
						return cells;
					}
					j++;
				}
			}
			choices_n = choices_hi-choices_lo;
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
			r = cell;
			for (i = choices_lo; i < choices_hi && r >= cell; i++) {
				int symmetric_new = set_symmetric(cell, symmetric, cell_sym, choices[i].letter_hor);
				if (symmetric_new == -1) {
					continue;
				}
				copy_choice(cell, choices+i);
				if (choices[i].letter_hor->symbol != '#') {
					choices[i].letter_hor->used++;
					if (choices[i].letter_ver->used < choices[i].letter_ver->leafs_n) {
						choices[i].letter_ver->used++;
						r = solve_white(choices[i].letter_ver, cell, choices_hi, blacks_n1, blacks_n2, symmetric_new, solve_grid);
						choices[i].letter_ver->used--;
					}
					choices[i].letter_hor->used--;
				}
				else {
					if (node_hor != root) {
						choices[i].letter_hor->used++;
					}
					if (choices[i].letter_ver->used < choices[i].letter_ver->leafs_n) {
						if (node_ver != root) {
							choices[i].letter_ver->used++;
						}
						r = solve_black(choices[i].letter_ver, black_in_cols[cell->col], cell, choices_hi, blacks_n1, blacks_n2, symmetric_new, solve_grid);
						if (node_ver != root) {
							choices[i].letter_ver->used--;
						}
					}
					if (node_hor != root) {
						choices[i].letter_hor->used--;
					}
				}
				cell->symbol = '.';
			}
			return r;
		}
		else {
			return solve_grid_final(node_hor, cell+2, choices_lo, blacks_n1, blacks_n2, symmetric);
		}
	}
	else {
		if (cell->col < cols_n) {
			node_t *node_ver = (cell-cols_offset)->node_ver;
			return solve_grid_final(node_ver, cell+1, choices_lo, blacks_n1, blacks_n2, symmetric);
		}
		else {
			blacks_max = blacks_n1+blacks_n2;
			puts("SOLUTION");
			print_grid(blacks_n1, blacks_n2);
			return cell;
		}
	}
}

cell_t *solve_line(cell_t *cell, int choices_lo, int blacks_n1, int blacks_n2, int symmetric) {
	if (cell->col > col_max) {
		col_max = cell->col;
	}
	if (!check_blacks(cell, blacks_n1, blacks_n2)) {
		return cell;
	}
	if (cell->row < rows_n) {
		node_t *node_hor = (cell-1)->node_hor;
		if (cell->col < cols_n) {
			int choices_hi = choices_lo, choices_n, i, j;
			node_t *node_ver = (cell-cols_offset)->node_ver;
			cell_t *cell_sym = cells+(cell->col+1)*cols_offset+cell->row+1, *r;
			for (i = 0, j = 0; i < node_hor->letters_n; i++) {
				if (cell->col+node_hor->letters[i].len_min > cols_n || (cell->col+node_hor->letters[i].len_max < cols_n && blacks_n1+blacks_n2+1 >= blacks_max)) {
					continue;
				}
				while (j < node_ver->letters_n && node_ver->letters[j].symbol < node_hor->letters[i].symbol) {
					j++;
				}
				while (j < node_ver->letters_n && node_ver->letters[j].symbol == node_hor->letters[i].symbol) {
					if (cell->row+node_ver->letters[j].len_min <= rows_n && (cell->row+node_ver->letters[j].len_max >= rows_n || blacks_n1+blacks_n2+1 < blacks_max) && !add_choice(node_hor->letters+i, node_ver->letters+j, &choices_hi)) {
						return cells;
					}
					j++;
				}
			}
			choices_n = choices_hi-choices_lo;
			qsort(choices+choices_lo, (size_t)choices_n, sizeof(choice_t), compare_choices);
			r = cell;
			for (i = choices_lo; i < choices_hi && r > cells && col_max < cols_n+1; i++) {
				int symmetric_new = set_symmetric(cell, symmetric, cell_sym, choices[i].letter_hor);
				if (symmetric_new == -1) {
					continue;
				}
				copy_choice(cell, choices+i);
				if (choices[i].letter_hor->symbol != '#') {
					r = solve_white(choices[i].letter_ver, cell, choices_hi, blacks_n1, blacks_n2, symmetric_new, solve_line);
				}
				else {
					r = solve_black(choices[i].letter_ver, black_in_cols[cell->col], cell, choices_hi, blacks_n1, blacks_n2, symmetric_new, solve_line);
				}
				cell->symbol = '.';
			}
			return r;
		}
		else {
			if (get_letter(node_hor, '#', 0) || get_letter(node_hor, '#', 1)) {
				col_max = cols_n+1;
			}
			return cell;
		}
	}
	else {
		if (cell->col < cols_n) {
			node_t *node_ver = (cell-cols_offset)->node_ver;
			if (get_letter(node_ver, '#', 0) || get_letter(node_ver, '#', 1)) {
				return solve_line(cell+1, choices_lo, blacks_n1, blacks_n2, symmetric);
			}
		}
		else {
			col_max = cols_n+1;
		}
		return cell;
	}
}

int check_blacks(cell_t *cell, int blacks_n1, int blacks_n2) {
	return blacks_n1+blacks_n2 < blacks_max && (!linear_blacks || (cell->row == 0 && cell->col == 0) || (double)blacks_n1/(cell->row*cols_n+cell->col) <= (double)blacks_max/(rows_n*cols_n));
}

int add_choice(letter_t *letter_hor, letter_t *letter_ver, int *choices_hi) {
	if (*choices_hi == choices_max) {
		choice_t *choices_tmp = realloc(choices, sizeof(choice_t)*(size_t)(*choices_hi+1));
		if (!choices_tmp) {
			fprintf(stderr, "Could not reallocate memory for choices\n");
			fflush(stderr);
			return 0;
		}
		choices = choices_tmp;
		choices_max = *choices_hi+1;
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

int set_symmetric(cell_t *cell, int symmetric, cell_t *cell_sym, letter_t *letter) {
	if (!symmetric || cell->row <= cell->col) {
		return symmetric;
	}
	if (letter->symbol < cell_sym->symbol) {
		return -1;
	}
	if (letter->symbol > cell_sym->symbol) {
		return 0;
	}
	return 1;
}

void copy_choice(cell_t *cell, choice_t *choice) {
	cell->node_hor = choice->letter_hor->next;
	cell->node_ver = choice->letter_ver->next;
	cell->symbol = choice->letter_hor->symbol;
}

cell_t *solve_white(letter_t *letter, cell_t *cell, int choices_lo, int blacks_n1, int blacks_n2, int symmetric, cell_t *(*solve_fn)(cell_t *, int, int, int, int)) {
	cell_t *r;
	if (cell->row+letter->len_max < rows_n && black_in_cols[cell->col] == rows_n) {
		black_in_cols[cell->col] = cell->row;
		blacks_n2++;
	}
	r = solve_fn(cell+1, choices_lo, blacks_n1, blacks_n2, symmetric);
	if (cell->row+letter->len_max < rows_n && black_in_cols[cell->col] == cell->row) {
		blacks_n2--;
		black_in_cols[cell->col] = rows_n;
	}
	return r;
}

cell_t *solve_black(letter_t *letter, int black_in_col, cell_t *cell, int choices_lo, int blacks_n1, int blacks_n2, int symmetric, cell_t *(*solve_fn)(cell_t *, int, int, int, int)) {
	cell_t *r;
	if (cell->row+letter->len_max < rows_n && black_in_col < rows_n) {
		black_in_cols[cell->col] = rows_n;
		blacks_n2--;
	}
	r = solve_fn(cell+1, choices_lo, blacks_n1+1, blacks_n2, symmetric);
	if (cell->row+letter->len_max < rows_n && black_in_col < rows_n) {
		blacks_n2++;
		black_in_cols[cell->col] = black_in_col;
	}
	return r;
}

cell_t *solve_grid_final(node_t *node, cell_t *cell, int choices_lo, int blacks_n1, int blacks_n2, int symmetric) {
	letter_t *letter = get_letter(node, '#', 0);
	if (!letter || letter->used >= letter->leafs_n) {
		letter = get_letter(node, '#', 1);
	}
	if (letter && letter->used < letter->leafs_n) {
		cell_t *r;
		if (node != root) {
			letter->used++;
		}
		r = solve_grid(cell, choices_lo, blacks_n1, blacks_n2, symmetric);
		if (node != root) {
			letter->used--;
		}
		return r;
	}
	return cell;
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
		putchar(cells[i*cols_offset+1].symbol);
		for (j = 2; j <= cols_n; j++) {
			printf(" %c", cells[i*cols_offset+j].symbol);
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
