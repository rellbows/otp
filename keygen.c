/*
 * Filename: keygen.c
 * Description: Creates a key file with randomly created characters for use
 * in creating ciphertext.
 * Class: CS344 - Operating Systems
*/

#include "numKey.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int randInt(int min, int max);

int main(int argc, char *argv[]){

	// silence warning for argc
	(void)argc;

	// variable setup
	int keyLength = atoi( argv[1] );
	int i;

	// Use current time to seed random generator
	srand(time(0));

	// testing
	for(i = 0; i < keyLength; i++){
		printf("%c", numToKey(randInt(0, 27)));
		fflush(stdout);
	}
	printf("\n");

	return 0;
}

// Description: generates a random integer 
// Argument(s): takes a min (inclusive) and max (exclusive) range
// Returns: a randomly generated integer
//  Citations: used below 'geeksforgeeks' article as reference
//  https://www.geeksforgeeks.org/generating-random-number-range-c/
int randInt(int min, int max){
	
	return (rand() % (max - min)) + min;
}
