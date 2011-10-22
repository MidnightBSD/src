static void
second_func (void)
{
    here_comes_nothing_in_a ();
}

int
main (int argc, char **argv)
{
    /* Call some function defined in b.c */
    invoke_bc ();
    second_func ();
    return 0;
}
