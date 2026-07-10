#
# SPDX-License-Identifier: BSD-2-Clause
#

atf_test_case encode_stdin
encode_stdin_head()
{
	atf_set "descr" "encode standard input"
}
encode_stdin_body()
{
	printf "hello" >input
	printf "aGVsbG8=\n" >expected

	atf_check -s exit:0 -e empty -o file:expected base64 <input
}

atf_test_case encode_file
encode_file_head()
{
	atf_set "descr" "encode a file operand"
}
encode_file_body()
{
	printf "hello" >input
	printf "aGVsbG8=\n" >expected

	atf_check -s exit:0 -e empty -o file:expected base64 input
}

atf_test_case decode
decode_head()
{
	atf_set "descr" "decode base64 input"
}
decode_body()
{
	printf "aGVsbG8=\n" >input
	printf "hello" >expected

	atf_check -s exit:0 -e empty -o file:expected base64 -d input
	atf_check -s exit:0 -e empty -o file:expected base64 --decode input
}

atf_test_case wrap
wrap_head()
{
	atf_set "descr" "wrap encoded output"
}
wrap_body()
{
	printf "abcdef" >input
	printf "YWJj\nZGVm\n" >expected
	printf "YWJjZGVm" >nowrap

	atf_check -s exit:0 -e empty -o file:expected base64 -w 4 input
	atf_check -s exit:0 -e empty -o file:expected base64 --wrap=4 input
	atf_check -s exit:0 -e empty -o file:nowrap base64 -w 0 input
}

atf_test_case ignore_garbage
ignore_garbage_head()
{
	atf_set "descr" "ignore garbage while decoding when requested"
}
ignore_garbage_body()
{
	printf "aG!VsbG8=\n" >input
	printf "hello" >expected

	atf_check -s exit:1 -e match:"invalid input" -o empty \
	    base64 -d input
	atf_check -s exit:0 -e empty -o file:expected base64 -d -i input
	atf_check -s exit:0 -e empty -o file:expected \
	    base64 --decode --ignore-garbage input
}

atf_test_case invalid_wrap
invalid_wrap_head()
{
	atf_set "descr" "reject invalid wrap values"
}
invalid_wrap_body()
{
	printf "hello" >input

	atf_check -s exit:1 -e match:"invalid wrap size" -o empty \
	    base64 -w nope input
}

atf_test_case dash_file
dash_file_head()
{
	atf_set "descr" "a dash operand reads standard input"
}
dash_file_body()
{
	printf "hello" >input
	printf "aGVsbG8=\n" >expected

	atf_check -s exit:0 -e empty -o file:expected base64 - <input
}

atf_init_test_cases()
{
	atf_add_test_case encode_stdin
	atf_add_test_case encode_file
	atf_add_test_case decode
	atf_add_test_case wrap
	atf_add_test_case ignore_garbage
	atf_add_test_case invalid_wrap
	atf_add_test_case dash_file
}
