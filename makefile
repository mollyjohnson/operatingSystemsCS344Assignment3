#Name: Molly Johnson
#ONID: johnsmol
#CS 344 Fall 2018
#Due: 11/14/18

all: smallsh

# -std=c99 allowed for this assignment per the assignment 3 instructions
smallsh: smallsh.c
	gcc -std=c99 -Wall -g -o smallsh smallsh.c

clean:
	rm smallsh out* junk*

cleanall:
	rm smallsh out* junk*
