fun mod(uint32 a, uint32 divisor->uint32)
{
	while (a >= divisor)
	{
		a = a - divisor;
	}
	return a;
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
