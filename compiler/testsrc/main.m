#include "math.h"
// #include "print.h"
/*
fun test(->void)
{
	uint8 i = 1;
	while(i < 15)
	{
		uint8 j = 1;
		while(j < 15)
		{
			uint8 printedDigits = printNum(mul(i, j), 0);
			while(printedDigits < 5)
			{
				print(32);
				printedDigits = printedDigits + 1;
			}
			j = j + 1;
		}
		print(10);
		i = i + 1;
	}
	print(10);
}

fun fib(uint8 n->uint32)
{
	if(n > 1)
	{
		return fib(n - 1) + fib(n - 2);
	}
	return n;
}
*/

fun test(->void)
{

}

fun main(->void)
{
	test();
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