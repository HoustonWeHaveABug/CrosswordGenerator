# CrosswordGenerator

crossword_gen is a program written in C allowing to generate crosswords from a list of words.

The following parameters are expected on the standard input:

- Number of rows (> 0)
- Number of columns (>= Number of rows, Number of cells <= 65536)
- Minimum number of black squares (>= 0)
- Maximum number of black squares (>= Minimum number of black squares, <= Number of cells)
- Heuristic (0: frequency, 1: random, > 1: none)
- Maximum number of choices at each step (> 0)
- Options (= sum of the below flags)
  - Black squares symmetry (0: disabled, 1: enabled)
  - White squares connected (0: disabled, 2: enabled)
  - Linear blacks (0: disabled, 4: enabled)
- \[ RNG seed \]

The path to the list of words to be used by the program is expected as an argument. In the list, each word must be written in lowercase or uppercase (no space or special character allowed), one word per line.

#### Example (program executed under Linux)

$ echo 10 15 10 25 1 2 3 123456789 | crossword_gen my_words.txt

Means "Generate a 10x15 crossword, with 10 black squares at least, 25 black squares at most, use random heuristic, start with a maximum of 2 choices at each step, enable black squares symmetry, enable white squares connected, disable linear blacks. Use 123456789 as the random seed. Use the file my_words.txt as the list of words."

The program generates a trie from the list of words provided. The crossword is generated cell by cell, in a row scan way. At each step the program determines the list of possible letters and if a black square can be placed from the current horizontal and vertical nodes in the trie.

When a valid solution is found, the current number of black squares becomes the new maximum. If the new maximum is less than the minimum number of black squares then the program terminates, otherwise another solution is searched using the new maximum. A solution is valid if all the words found are in the list of words (including one-letter words), and there is no duplicate words on the grid.

When the Frequency heuristic is used, the program will sort the list of possible choices at each cell using their frequency of usage. When the Random heuristic is used, a shuffle of those choices is performed.

The program will use the value provided for Maximum number of choices at each step to limit the size of the search tree. If it is impossible to generate a crossword with the initial value provided for this parameter, then it is incremented and the search starts again until a solution is found or all the possible choices were tried.

When the Black squares symmetry option is enabled, the program will ensure black squares are placed respecting the 180-degree rotational symmetry constraint. Otherwise there is no constraint on the placement of black squares.

When the White squares connected option is enabled, the program will ensure all white squares are connected to each other. Otherwise there is no constraint on the placement of white squares.

When the Linear blacks option is enabled, the program will try to put a black square at most every (Number of rows \* Number of colums)/(Maximum number of black squares) cells. It means in this case the program may miss valid solutions. Otherwise a full search is always performed.

When a RNG seed is provided on the standard input it will be used to seed the Mersenne Twister RNG. Otherwise the result of the time() function will be used.
