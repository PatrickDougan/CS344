#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(int argc, char *argv[])
{
	srand(time(0));
	int x, y;

	int keyLength = atoi(argv[1]);
	char *str = malloc(sizeof(char) * keyLength);
	
	for(x = 0; x < keyLength; x++)
	{
		y = (rand() % 27);
		if(y == 0)
		{
			y = 32;
		}
		else
		{
			y += 64;
		}
		str[x] = y;
	}
	
	
	fprintf(stdout, "%s\n",str);
	
	free(str);	
	return 0;	
}
