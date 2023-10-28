
class Stack
{
    u32 data[16];
    u32 size;
};

fun Stack_Initialize(class Stack *s->void);

fun Stack_Push(class Stack *s, u32 data->void);

fun Stack_Pop(class Stack *s->u32);
