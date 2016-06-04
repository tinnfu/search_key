# search_key
1. search key from file
2. show line-info with line NO.

# usage
Usage: {proc} <-f file> [-k key, keyword to search from file] [-l line, line to print info in file]

# platform
OS: LINUX  
CODE: C

# version
simple demo just for fun

# bugs
1. do not load all file info from file to search and show
2. other bugs, not stable

# example
## test.txt
asdf45asd78asdf000  
1  
2  
3  
4  

## search key
* {proc} -f test.txt -k asdf

* result:  
0~3  
9~12  

## show line
* {proc} -f test.txt -l 1
* result:  
asdf45asd78asdf000

