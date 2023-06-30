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

fun div(uint32 a, uint32 b->uint32)
{
	uint32 quotient = 0;
	while (a >= b)
	{
		a = a - b;
		quotient = quotient + 1;
	}
	return quotient;
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
	while (i <= digits)
	{
		print(outStr[digits - i]);
		i = i + 1;
	}

	if (newLine > 0)
	{
		digits = digits + 1;
		print(10); // newline
	}
	return digits;
}

uint32 nMultiplications = 0;

fun mul(uint32 a, uint32 b->uint32)
{
	uint32 result = 0;
	while(b > 0)
	{
		result = result + a;
		b = b - 1;
	}
	nMultiplications = nMultiplications + 1;
	return result;
}

fun test(->void)
{
	uint8 i = 1;
	while(i < 10)
	{
		uint8 j = 1;
		while(j < 10)
		{
			uint8 printedDigits = printNum(mul(i, j), 0);
			while(printedDigits < 4)
			{
				print(' ');
				printedDigits = printedDigits + 1;
			}
			j = j + 1;
		}
		print(10);
		i = i + 1;
	}
	print(10);
	printNum(nMultiplications, 1);
}

fun main(->void)
{
	test();
}

asm
	{
	code:
	call main
	hlt
}
;