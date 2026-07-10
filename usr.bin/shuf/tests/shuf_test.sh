#
# SPDX-License-Identifier: BSD-2-Clause
#

LC_ALL=C
export LC_ALL

check_lines()
{
	file=$1
	expected=$2
	lines=$(wc -l <"${file}" | tr -d ' ')

	if [ "${lines}" != "${expected}" ]; then
		atf_fail "expected ${expected} lines, got ${lines}"
	fi
}

atf_test_case echo_args
echo_args_head()
{
	atf_set "descr" "shuffle command-line arguments"
}
echo_args_body()
{
	printf "alpha\nbeta\ngamma\n" >expected

	atf_check -s exit:0 -e empty -o save:actual \
	    shuf -e alpha beta gamma
	sort actual >actual.sorted
	atf_check -o file:expected cat actual.sorted
}

atf_test_case input_file
input_file_head()
{
	atf_set "descr" "shuffle lines from an input file"
}
input_file_body()
{
	printf "alpha\nbeta\ngamma\n" >input
	printf "alpha\nbeta\ngamma\n" >expected

	atf_check -s exit:0 -e empty -o save:actual shuf input
	sort actual >actual.sorted
	atf_check -o file:expected cat actual.sorted
}

atf_test_case input_stdin
input_stdin_head()
{
	atf_set "descr" "shuffle lines from standard input"
}
input_stdin_body()
{
	printf "alpha\nbeta\ngamma\n" >input
	printf "alpha\nbeta\ngamma\n" >expected

	atf_check -s exit:0 -e empty -o save:actual shuf <input
	sort actual >actual.sorted
	atf_check -o file:expected cat actual.sorted
}

atf_test_case head_count
head_count_head()
{
	atf_set "descr" "-n limits output count"
}
head_count_body()
{
	atf_check -s exit:0 -e empty -o save:actual \
	    shuf -e -n 2 alpha beta gamma
	check_lines actual 2
	atf_check -s exit:1 -e empty grep -v -E '^(alpha|beta|gamma)$' actual
}

atf_test_case integer_range
integer_range_head()
{
	atf_set "descr" "-i shuffles an integer range"
}
integer_range_body()
{
	printf "3\n4\n5\n" >expected

	atf_check -s exit:0 -e empty -o save:actual shuf -i 3-5
	sort -n actual >actual.sorted
	atf_check -o file:expected cat actual.sorted
}

atf_test_case output_file
output_file_head()
{
	atf_set "descr" "-o writes to the requested output file"
}
output_file_body()
{
	atf_check -s exit:0 -e empty -o empty \
	    shuf -e -n 1 -o actual alpha beta
	check_lines actual 1
	atf_check -s exit:1 -e empty grep -v -E '^(alpha|beta)$' actual
}

atf_test_case zero_delimiter
zero_delimiter_head()
{
	atf_set "descr" "-z reads and writes NUL-delimited records"
}
zero_delimiter_body()
{
	printf "alpha\0beta\0gamma\0" >input
	printf "alpha\nbeta\ngamma\n" >expected

	atf_check -s exit:0 -e empty -o save:actual shuf -z input
	tr '\0' '\n' <actual | sort >actual.sorted
	atf_check -o file:expected cat actual.sorted
}

atf_init_test_cases()
{
	atf_add_test_case echo_args
	atf_add_test_case input_file
	atf_add_test_case input_stdin
	atf_add_test_case head_count
	atf_add_test_case integer_range
	atf_add_test_case output_file
	atf_add_test_case zero_delimiter
}
