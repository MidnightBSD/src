#
# Basic regression tests for ps(1) terminal width handling.
#

atf_test_case columns_negative_unlimited
columns_negative_unlimited_body()
{
	PS_BIN="$(ls /usr/obj/usr/src/*/bin/ps/ps 2>/dev/null | head -n 1)"
	if [ -z "${PS_BIN}" ] || [ ! -x "${PS_BIN}" ]; then
		PS_BIN="ps"
	fi

	# With invalid/negative COLUMNS, ps should treat width as unlimited
	# (no wrapping within showkey output).
	atf_check -o match:"^[[:space:]]*1$" sh -c \
	    "env COLUMNS=-1 \"${PS_BIN}\" -L | wc -l"
	atf_check -o match:"^[[:space:]]*[0-9]+" \
	    env COLUMNS=-1 "${PS_BIN}" -o pid= -p $$
}

atf_test_case columns_small_wraps
columns_small_wraps_body()
{
	PS_BIN="$(ls /usr/obj/usr/src/*/bin/ps/ps 2>/dev/null | head -n 1)"
	if [ -z "${PS_BIN}" ] || [ ! -x "${PS_BIN}" ]; then
		PS_BIN="ps"
	fi

	# With a very small COLUMNS, showkey should wrap to multiple lines.
	atf_check -o not-empty sh -c "env COLUMNS=1 \"${PS_BIN}\" -L"
	lines="$(env COLUMNS=1 "${PS_BIN}" -L | wc -l)"
	atf_check -s exit:0 test "${lines}" -gt 1
}

atf_test_case columns_overflow_input
columns_overflow_input_body()
{
	PS_BIN="$(ls /usr/obj/usr/src/*/bin/ps/ps 2>/dev/null | head -n 1)"
	if [ -z "${PS_BIN}" ] || [ ! -x "${PS_BIN}" ]; then
		PS_BIN="ps"
	fi

	# Huge values should not cause overflow-related misbehavior or crashes.
	atf_check -s exit:0 -o not-empty env COLUMNS=999999999999999999999 "${PS_BIN}" -L
}

atf_init_test_cases()
{
	atf_add_test_case columns_negative_unlimited
	atf_add_test_case columns_small_wraps
	atf_add_test_case columns_overflow_input
}
