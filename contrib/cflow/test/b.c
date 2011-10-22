void
first_func (void)
{
    int a = 0;
    int b = 0;
    int c = 0;

    c = a + b;
}

static void
second_func ()
{
    do_nada ();
}
void
invoke_bc ()
{
    second_func ();
}
