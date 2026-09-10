#
# SPDX-License-Identifier: BSD-2-Clause
#

atf_test_case literal_format
literal_format_head()
{
	atf_set "descr" "prepend a literal timestamp format to every line"
}
literal_format_body()
{
	printf "stamp alpha\nstamp beta\n" >expected

	atf_check -s exit:0 -e empty -o file:expected \
	    sh -c "printf 'alpha\\nbeta\\n' | ts stamp"
}

atf_test_case unterminated_line
unterminated_line_head()
{
	atf_set "descr" "preserve an unterminated final input line"
}
unterminated_line_body()
{
	printf "stamp alpha" >expected

	atf_check -s exit:0 -e empty -o file:expected \
	    sh -c "printf alpha | ts stamp"
}

atf_test_case empty_input
empty_input_head()
{
	atf_set "descr" "produce no timestamp for empty input"
}
empty_input_body()
{
	atf_check -s exit:0 -e empty -o empty ts stamp </dev/null
}

atf_test_case elapsed
elapsed_head()
{
	atf_set "descr" "support elapsed timestamps with microseconds"
}
elapsed_body()
{
	atf_check -s exit:0 -e empty -o match:'^00\.[0-9]{6} alpha$' \
	    sh -c "printf 'alpha\\n' | ts -s %.S"
}

atf_test_case conflicting_options
conflicting_options_head()
{
	atf_set "descr" "reject simultaneous interval and elapsed modes"
}
conflicting_options_body()
{
	atf_check -s exit:1 -e match:usage -o empty ts -i -s
}

atf_init_test_cases()
{
	atf_add_test_case literal_format
	atf_add_test_case unterminated_line
	atf_add_test_case empty_input
	atf_add_test_case elapsed
	atf_add_test_case conflicting_options
}
