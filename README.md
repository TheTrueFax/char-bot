# char-bot
char-bot is a CLI tool that can "train" off of files and write it to an SModel file, and read from an SModel file to recursively predict the next byte in a data stream.

# Usage
Help menu:
```
-i [file]   |  specify input file
-o [file]   |  creates or writes to a file for output
-l  [i]     |  set target length for output, or change target training memory size
-t, --train |  changes the mode to training mode, where it will create a model
-w, --write |  do not print generated text, write to the output file instead
-h, --help  |  show this menu
````

## Examples:
`./cbot --train -i file.txt -o trained.smodel -l 5` - Train from "file.txt" into "trained.smodel" with a memory size of 5

`./cbot -i trained.smodel -l 100` - Generate text with a target length of 100 bytes from "trained.smodel", and print the generated text.

`./cbot -i trained.smodel -l 5000 --write -o generated.txt` - Generate text with a target length of 5kb from "trained.smodel", and write the output to "generated.txt"

# Building
Building is extremely simple, the app has only been built on command-line GCC on debian linux.

`gcc main.c -o cbot` To build.

# .smodel File format
(indented types are repeated groups)
```
uint8 - version
uint16 - name_length
char[] - name
uint8 - before_length (memory size)
uint32 - lc_count (amount of memory groups)
    uint32 - letter_count
    char[] - before (memory)
    char[] - after (possible next letters)
```

# Theory
lc_count corresponds to previously mentioned "Memory size", and controls how many bytes the next byte is chosen from. This makes a larger lc_count lead to more coherent data.
Training goes through every byte in a file and keeps a "memory" the size of lc_count. When training, the memory is saved to an array, if the memory is not in the array yet, it will add it along with the current letter, but if the the memory already exists in the array, it adds the current letter to the possible next letters for that memory.
And on generation it chooses a random next letter from the possible next letters from the matching memory, making more common combinations in the original text more common to be generated.
