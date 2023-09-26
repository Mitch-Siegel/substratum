# Source code

```
# this function computes the nth fibonacci number 
fun fib(var n)
{
    if(n < 2)
        return n;
    else
        return fib(n - 1) + fib(n - 2); 
}

# find all fibonacci numbers up to and including n
fun firstnfibs(var n)
{
    var i = 0;
    while(i < n)
    {
        fib(i + 1);
        asm
        {
            out %r0
        };
        i = i + 1;
    }
}
```

# Generated assembly

```
fib:
	sub %sp, $0
	push %r1
	mov %r0, 4(%bp)
	cmp %r0, $2
	jge fib_1
	mov %r0, 4(%bp)
	jmp fib_done
fib_1:
	mov %r0, 4(%bp)
	sub %r0, $2
	push %r0
	call fib
	mov %r1, 4(%bp)
	sub %r1, $1
	push %r1
	mov %r1, %r0
	call fib
	add %r1, %r0
	mov %r0, %r1
	jmp fib_done
fib_done:
	pop %r1
	add %sp, $0
	ret 1
firstnfibs:
	sub %sp, $2
	push %r1
	push %r2
	mov %r0, $0
firstnfibs_1:
	cmp %r0, 4(%bp)
	jge firstnfibs_2
	mov %r1, %r0
	add %r1, $1
	push %r1
	mov %r2, %r0
	call fib
	out %r0
	add %r2, $1
	mov %r0, %r2
	jmp firstnfibs_1
firstnfibs_2:
firstnfibs_done:
	pop %r2
	pop %r1
	add %sp, $2
	ret 1
```
# Results

call to fib with argument of 20: 328363 instructions

call to firstnfibs with argument of 20: 859483 instructions