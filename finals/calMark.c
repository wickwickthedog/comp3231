#include <stdlib.h>
#include <stdio.h>

int main()
{
	int E, C;
	
	printf("Insert Course Marks :\n");
	scanf("%d", &C);

	printf("Insert Estimate Exam Marks :\n");
	scanf("%d", &E);

	int M = (5*E*C) / (2*E + 3*C);

	if (M < 50) printf("You Failed: %d\n", M);
	else printf("You Passed: %d\n", M);
}