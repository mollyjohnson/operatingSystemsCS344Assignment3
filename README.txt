/*
Name: Molly Johnson
ONID: johnsmol
CS 344 Winter 2019
Assignment 3
Due: 3/3/19
Adapted from my own work 11/14/18 (took the class in the Fall 2018 term
#but am retaking this term for a better grade)
*/

To compile my smallsh.c code, make sure smallsh.c and the included makefile are in the same directory
on os1. You can then simply type "make" and hit enter, and the makefile commands will get the code compiled
for you.  Alternatively, you can compile the smallsh.c file by putting the file in some directory on 
os1 and then typing "gcc -std=c99 -Wall -g -o smallsh smallsh.c" and hitting enter, as those are the exact same
compiling instructions as are present in the makefile. After compiling, the smallsh executable can be used with
the p3testscript file to test my program. Note: p3testscript file not included per the assignment instructions.
You can then just type "make clean" and hit enter to remove the executable smallsh file after compiling if desired.
Or if you want to remove any "junk" or "out" files in addition to the executable smallsh file, you can just type
"make cleanall" and hit enter.
