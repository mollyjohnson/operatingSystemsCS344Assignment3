#Name: Molly Johnson
#ONID: johnsmol
#CS 344 Winter 2019
#Due: 3/3/19
#Adapted from my own work 11/14/18 (took the class in the Fall 2018 term
#but am retaking this term for a better grade)

#have make all be the default target, with the dependency of smallsh
all: smallsh

#smallsh target with the dependency smallsh.c and command
#gcc -std=c99 -Wall -g -o smallsh smallsh.c.
#(-std=c99 allowed for this assignment per the assignment 3 instructions).
#command includes C99 flag, -Wall flag (so can see more compiler warnings), 
#-g flag (so can use gdb for debugging), and -o flag (tells it to write 
#the build output to the specified output file, i.e. smallsh in this case).
smallsh: smallsh.c
	gcc -std=c99 -Wall -g -o smallsh smallsh.c
	
#command to remove smallsh executable if user enters make clean.
#-f means don't ask if the file should be deleted, just delete it
clean:
	rm -f smallsh

#command to remove smallsh executable as well as any "out" or "junk" files
#if user enters make cleanall.
#-f means don't ask if the file should be deleted, just delete it
cleanall:
	rm -f smallsh out* junk*
