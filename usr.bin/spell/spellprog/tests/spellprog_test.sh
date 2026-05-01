#
# Regression tests for spellprog(1) robustness.
#

create_wordlist()
{
	cat > wordlist <<'EOF'
a
an
the
EOF
}

atf_test_case short_word_no_oob
short_word_no_oob_body()
{
	SPELLPROG_BIN="$(ls /usr/obj/usr/src/*/usr.bin/spell/spellprog/spellprog 2>/dev/null | head -n 1)"
	if [ -z "${SPELLPROG_BIN}" ] || [ ! -x "${SPELLPROG_BIN}" ]; then
		atf_skip "spellprog binary not found"
	fi

	create_wordlist
	# "a" should be found, and we should not crash on very short inputs.
	atf_check -s exit:0 sh -c "printf 'a\n' | \"${SPELLPROG_BIN}\" wordlist >/dev/null"
}

atf_test_case long_word_eof_no_hang
long_word_eof_no_hang_body()
{
	SPELLPROG_BIN="$(ls /usr/obj/usr/src/*/usr.bin/spell/spellprog/spellprog 2>/dev/null | head -n 1)"
	if [ -z "${SPELLPROG_BIN}" ] || [ ! -x "${SPELLPROG_BIN}" ]; then
		atf_skip "spellprog binary not found"
	fi

	create_wordlist
	# Provide a word longer than LINE_MAX without a trailing newline.
	# The program should not spin forever while slurping to EOL.
	atf_check -s exit:0 sh -c \
	    "jot -b a -s '' 5000 | tr -d '\n' | \"${SPELLPROG_BIN}\" wordlist >/dev/null 2>/dev/null"
}

atf_init_test_cases()
{
	atf_add_test_case short_word_no_oob
	atf_add_test_case long_word_eof_no_hang
}
