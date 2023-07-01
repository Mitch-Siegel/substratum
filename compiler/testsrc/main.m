#include "math.h"

fun print(uint8 value->void)
{
	// store r0, pull the argument into a register, then output that register
	asm {
		movb %r0, (%bp+8)
		out 0, %r0
	}
	;
}


fun printNum(uint32 num, uint32 newLine->uint8)
{
	uint8 outStr[16];
	uint32 digits = 0;
	if (num == 0)
	{
		outStr[0] = '0';
		digits = 1;
	}
	else
	{
		while (num > 0)
		{
			uint32 remainder = mod(num, 10);
			outStr[digits] = remainder + 48;
			num = div(num, 10);
			digits = digits + 1;
		}
	}

	uint32 i = 0;
	while (i < digits)
	{
		print(outStr[digits - i - 1]);
		i = i + 1;
	}

	if (newLine > 0)
	{
		digits = digits + 1;
		print(10); // newline
	}
	return digits;
}

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

fun main(->void)
{
	test();
	uint8 i = 0;
	while(i < 10)
	{
		printNum(fib(i), 1);
		i = i + 1;
	}
}

asm
	{
	code:
	call main
	hlt
}
;