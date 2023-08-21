#include "math.h"
#include "print.h"


fun test(uint8 n->void)
{
	uint8 i = 0;
	while(i < n)
	{
		printNum(fib(i), 0);
		i = i + 1;
		if(i < n)
		{
			print(',');
		}
	}
	print(10);
}

fun main(->void)
{
	test(20);
	/*uint8 i = 0;
	while(i < 10)
	{
		printNum(fib(i), 1);
		i = i + 1;
	}*/
}

asm
	{
	code:
	call main
	hlt
}
;