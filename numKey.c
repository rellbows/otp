#include "numKey.h"
#include <stdio.h>
#include <stdlib.h>

const int MAXKEYINDEX = 27;

char NUMKEY[] = { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L' , 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', ' '};

int keyToNum(char key){

	// setup variables
	int i;

	for(i = 0; i < MAXKEYINDEX; i++ ){
		if(key == NUMKEY[i]){
			return i;
		}
	}
	
	// not found
	return -1;
}

char numToKey(int keyIndex){
	
	if(keyIndex < MAXKEYINDEX && keyIndex >= 0){
		return NUMKEY[keyIndex];
	}
	else{
		printf("Error! Invalid key index.\n");
		exit(1);
	}
}

int getMaxKeyIndex(){
	return MAXKEYINDEX;
}
