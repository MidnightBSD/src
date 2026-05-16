#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 MidnightBSD
#

atf_test_case A_flag
A_flag_head()
{
	atf_set "descr" "Verify that -A is equivalent to -et"
}

A_flag_body()
{
	printf "tab\tchar\nnon-print\001\nend" > input
	atf_check -o save:expected cat -et input
	atf_check -o file:expected cat -A input
}

atf_init_test_cases()
{
	atf_add_test_case A_flag
}
