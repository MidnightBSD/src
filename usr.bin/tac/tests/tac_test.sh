#
# SPDX-License-Identifier: BSD-2-Clause
#

atf_test_case basic
basic_head()
{
	atf_set "descr" "reverse lines from a regular file"
}
basic_body()
{
	printf "one\ntwo\nthree\n" >input
	printf "three\ntwo\none\n" >expected

	atf_check -s exit:0 -e empty -o file:expected tac input
}

atf_test_case stdin
stdin_head()
{
	atf_set "descr" "reverse lines from standard input"
}
stdin_body()
{
	printf "one\ntwo\nthree\n" >input
	printf "three\ntwo\none\n" >expected

	atf_check -s exit:0 -e empty -o file:expected tac <input
	atf_check -s exit:0 -e empty -o file:expected tac - <input
}

atf_test_case missing_newline
missing_newline_head()
{
	atf_set "descr" "preserve unterminated final line"
}
missing_newline_body()
{
	printf "one\ntwo\nthree" >input
	printf "threetwo\none\n" >expected

	atf_check -s exit:0 -e empty -o file:expected tac input
}

atf_test_case multiple_files
multiple_files_head()
{
	atf_set "descr" "reverse each input file independently"
}
multiple_files_body()
{
	printf "one\ntwo\n" >input1
	printf "three\nfour\n" >input2
	printf "two\none\nfour\nthree\n" >expected

	atf_check -s exit:0 -e empty -o file:expected tac input1 input2
}

atf_test_case empty_file
empty_file_head()
{
	atf_set "descr" "empty input produces empty output"
}
empty_file_body()
{
	: >input

	atf_check -s exit:0 -e empty -o empty tac input
}

atf_test_case missing_file
missing_file_head()
{
	atf_set "descr" "missing files fail but do not stop later operands"
}
missing_file_body()
{
	printf "one\ntwo\n" >input
	printf "two\none\n" >expected

	atf_check -s exit:1 -e match:"missing" -o file:expected \
	    tac missing input
}

atf_init_test_cases()
{
	atf_add_test_case basic
	atf_add_test_case stdin
	atf_add_test_case missing_newline
	atf_add_test_case multiple_files
	atf_add_test_case empty_file
	atf_add_test_case missing_file
}
