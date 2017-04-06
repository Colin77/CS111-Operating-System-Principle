#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
	char this[5];
	int len, i;
	int to_hash=0;
	int hash_value;
	int modulo = 4;
	len = strlen(argv[1]);
	strcpy(this, argv[1]);
	for(i=0; i<len; i++){
		to_hash = to_hash*10 + (this[i] - '0');
	}
	printf("before: %d\n", to_hash);
	hash_value = to_hash % modulo;
	printf("Result: %d\n", hash_value);
	exit(0);
}