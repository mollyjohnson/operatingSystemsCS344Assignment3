#Name: Molly Johnson
#ONID: johnsmol
#CS 344 Winter 2019
#Due: 3/3/19
#Adapted from my own work 11/14/18 (took the class in the Fall 2018 term
#but am retaking this term for a better grade)

all: smallsh

# -std=c99 allowed for this assignment per the assignment 3 instructions
smallsh: smallsh.c
	gcc -std=c99 -Wall -g -o smallsh smallsh.c
	
#for use on makefile turned in (don't delete any created files)
#clean:
#rm -f smallsh

#for use on makefile during development/testing
clean:
	rm -f smallsh out* junk*

cleanall:
	rm -f smallsh out* junk*
