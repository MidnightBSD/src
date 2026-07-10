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

atf_test_case separator
separator_head()
{
	atf_set "descr" "reverse records separated by a string"
}
separator_body()
{
	printf "one:two:three:" >input
	printf "three:two:one:" >expected

	atf_check -s exit:0 -e empty -o file:expected tac -s : input
	atf_check -s exit:0 -e empty -o file:expected \
	    tac --separator=: input
}

atf_test_case separator_missing_final
separator_missing_final_head()
{
	atf_set "descr" "preserve records without a final custom separator"
}
separator_missing_final_body()
{
	printf "one:two:three" >input
	printf "threetwo:one:" >expected

	atf_check -s exit:0 -e empty -o file:expected tac -s : input
}

atf_test_case before
before_head()
{
	atf_set "descr" "attach separator to the following record"
}
before_body()
{
	printf "one:two:three:" >input
	printf "::three:twoone" >expected

	atf_check -s exit:0 -e empty -o file:expected tac -b -s : input
	atf_check -s exit:0 -e empty -o file:expected \
	    tac --before --separator=: input
}

atf_test_case regex_separator
regex_separator_head()
{
	atf_set "descr" "reverse records separated by a basic regular expression"
}
regex_separator_body()
{
	printf "a12b345c" >input
	printf "cb345a12" >expected

	atf_check -s exit:0 -e empty -o file:expected \
	    tac -r -s "[0-9][0-9]*" input
}

atf_test_case nul_separator
nul_separator_head()
{
	atf_set "descr" "empty separator means NUL"
}
nul_separator_body()
{
	printf "a\\0b\\0c" >input
	printf "cb\\0a\\0" >expected

	atf_check -s exit:0 -e empty -o file:expected tac -s "" input
}

atf_test_case empty_regex_separator
empty_regex_separator_head()
{
	atf_set "descr" "empty regex separators are rejected"
}
empty_regex_separator_body()
{
	printf "one\ntwo\n" >input

	atf_check -s exit:1 -e match:"separator cannot be empty" -o empty \
	    tac -r -s "" input
}

atf_init_test_cases()
{
	atf_add_test_case basic
	atf_add_test_case stdin
	atf_add_test_case missing_newline
	atf_add_test_case multiple_files
	atf_add_test_case empty_file
	atf_add_test_case missing_file
	atf_add_test_case separator
	atf_add_test_case separator_missing_final
	atf_add_test_case before
	atf_add_test_case regex_separator
	atf_add_test_case nul_separator
	atf_add_test_case empty_regex_separator
}
