fun mod(uint32 a, uint32 divisor->uint32)
{
	while (a >= divisor)
	{
		a = a - divisor;
	}
	return a;
}