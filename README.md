# CrosswordGenerator

crossword_gen is a program written in C allowing to generate random crosswords from a list of words.

The following parameters are expected on the standard input:

- Number of rows (> 0)
- Number of columns (>= Number of rows)
- Maximum number of black squares (>= 0)
- Linear blacks flag (0: false, otherwise true)
- Heuristic flag (0: false, otherwise true)

The path to the list of words to be used by the program is expected as an argument. In the list, each word must be written in uppercase (no accentuated characters allowed), one word per line.

The program generates a trie from the list of words provided. The crossword is generated cell by cell, in a row scan way. At each cell the program determines the list of possible letters from the current horizontal and vertical nodes in the trie. When a valid solution is found, the new Maximum number of squares becomes the number of those found in this solution, until it reaches 0 and the program terminates.

When the Linear blacks flag is true, the program will try to put a black square at most every (Number of rows \* Number of colums)/(Maximum number of black squares) cells. It means in this case the program may miss valid solutions. Otherwise a full search is always performed.

When the Heuristic flag is true, the program will sort the list of possible letters at each cell using their frequency of usage. Otherwise a shuffle of those letters is performed.
