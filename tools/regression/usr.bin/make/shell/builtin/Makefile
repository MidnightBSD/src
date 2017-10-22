#
# In compat mode (the default without -j) a line that contains no shell
# meta characters and no shell builtins is not passed to the shell but
# executed directly. In our example the ls line without meta characters
# will really execute ls, while the line with meta characters will execute
# our special shell.
#
# $FreeBSD: release/10.0.0/tools/regression/usr.bin/make/shell/builtin/Makefile 146822 2005-05-31 14:13:07Z harti $

.SHELL: path="${.CURDIR}/sh"

.PHONY: builtin no-builtin

builtin:
	@exec ls -d .

no-builtin:
	@ls -d .
