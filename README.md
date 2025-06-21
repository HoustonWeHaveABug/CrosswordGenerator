# CrosswordGenerator

crossword_gen is a program written in C allowing to generate crosswords from a list of words.

The following parameters are expected on the standard input:

- Number of rows (> 0)
- Number of columns (>= Number of rows, Number of cells <= 65536)
- Minimum number of black squares (>= 0)
- Maximum number of black squares (>= Minimum number of black squares, <= Number of cells)
- Heuristic (0: weight, 1: weighted shuffle, 2: shuffle, > 2: none)
- Options (= sum of the below flags)
  - Symmetric black squares (0: disabled, 1: enabled)
  - Connected white squares (0: disabled, 2: enabled)
  - Linear black squares (0: disabled, 4: enabled)
  - Iterative choices (0: disabled, 8: enabled)
- \[ RNG seed \]

The path to the list of words to be used by the program is expected as an argument. In the list, each word must be written in lowercase or uppercase (no space or special character allowed), one word per line.

#### Example (program executed under Linux)

$ echo 10 15 10 25 1 11 123456789 | crossword_gen my_words.txt

Means "Generate a 10x15 crossword, with 10 black squares at least, 25 black squares at most, use random heuristic, enable symmetric black squares, enable connected white squares, disable linear black squares, enable iterative choices. Use 123456789 as the RNG seed. Use the file my_words.txt as the list of words."

The program generates a trie from the list of words provided. The crossword is generated cell by cell, in a row scan way. At each step the program determines the list of possible letters and if a black square can be placed from the current horizontal and vertical nodes in the trie.

When a valid solution is found, the current number of black squares - 1 becomes the new maximum. If the new maximum is less than the minimum number of black squares then the program terminates, otherwise another solution is searched using the new maximum. A solution is valid if all the words found are in the list of words (including one-letter words), and there is no duplicate words on the grid.

When the Weight heuristic is used, the program will sort the list of possible choices at each cell using the sum of the weights for the current horizontal and vertical nodes in the trie. When the Weighted Shuffle heuristic is used, the program will sort the list of possible choices using a random number between 0 and the sum of their weights (excluded). When the Shuffle heuristic is used, a shuffle of the possible choices is performed.

When the Symmetric black squares option is enabled, the program will ensure black squares are placed respecting the 180-degree rotational symmetry constraint. Otherwise there is no constraint on the placement of black squares.

When the Connected white squares option is enabled, the program will ensure all white squares are connected to each other. Otherwise there is no constraint on the placement of white squares.

When the Linear black squares option is enabled, the program will try to put a black square at most every (Number of rows \* Number of colums)/(Maximum number of black squares) cells. It means in this case the program may miss valid solutions. Otherwise a full search will be performed.

When the Iterative choices option is enabled, the search will start with the maximum number of choices tried at each step set to 1. If no solution is found then the maximum will be incremented and a new search started until a solution is found or all possible choices were tried. Otherwise all possible choices will be tried at each step.

When a RNG seed is provided on the standard input it will be used to seed the Mersenne Twister RNG. Otherwise the result of the time() function will be used.
