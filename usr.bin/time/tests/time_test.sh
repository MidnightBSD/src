#
# Copyright (c) 2026 Lucas Holt
#
# SPDX-License-Identifier: BSD-2-Clause
#

TIME=/usr/bin/time

atf_test_case format_literal
format_literal_head() {
	atf_set descr "-f copies literal text and appends a newline"
}
format_literal_body() {
	atf_check -s exit:0 -e inline:"hello world\n" \
	    ${TIME} -f "hello world" true
}

atf_test_case format_percent
format_percent_head() {
	atf_set descr "-f prints %% as a single percent sign"
}
format_percent_body() {
	atf_check -s exit:0 -e inline:"100%\n" \
	    ${TIME} -f "100%%" true
}

atf_test_case format_backslash
format_backslash_head() {
	atf_set descr "-f expands \\t, \\n and \\\\"
}
format_backslash_body() {
	printf 'a\tb\nc\\d\n' >expected
	atf_check -s exit:0 -e file:expected \
	    ${TIME} -f 'a\tb\nc\\d' true
}

atf_test_case format_unknown
format_unknown_head() {
	atf_set descr "-f prints unknown conversions as ?c"
}
format_unknown_body() {
	atf_check -s exit:0 -e inline:"?q\n" \
	    ${TIME} -f "%q" true
}

atf_test_case format_trailing
format_trailing_head() {
	atf_set descr "-f keeps a trailing % or backslash literal"
}
format_trailing_body() {
	atf_check -s exit:0 -e inline:"x%\n" ${TIME} -f "x%" true
	atf_check -s exit:0 -e inline:"x\\\\\n" ${TIME} -f 'x\' true
}

atf_test_case format_command
format_command_head() {
	atf_set descr "%C prints the command and its arguments"
}
format_command_body() {
	atf_check -s exit:0 -e inline:"sh -c exit 0\n" \
	    ${TIME} -f "%C" sh -c "exit 0"
}

atf_test_case format_exit_status
format_exit_status_head() {
	atf_set descr "%x prints the exit status of the command"
}
format_exit_status_body() {
	atf_check -s exit:0 -e inline:"0\n" ${TIME} -f "%x" true
	atf_check -s exit:3 -e inline:"3\n" ${TIME} -f "%x" sh -c "exit 3"
}

atf_test_case format_signal
format_signal_head() {
	atf_set descr "%x prints the signal number when the command is killed"
}
format_signal_body() {
	atf_check -s signal:15 -e match:"^15$" \
	    ${TIME} -f "%x" sh -c 'kill -TERM $$'
}

atf_test_case format_elapsed
format_elapsed_head() {
	atf_set descr "%e and %E report the elapsed time"
}
format_elapsed_body() {
	atf_check -s exit:0 -e match:"^[1-9][.,][0-9][0-9]$" \
	    ${TIME} -f "%e" sleep 1
	atf_check -s exit:0 -e match:"^0:0[1-9][.,][0-9][0-9]$" \
	    ${TIME} -f "%E" sleep 1
	atf_check -s exit:0 -e match:"^0[.,][0-9][0-9]$" \
	    ${TIME} -f "%e" true
}

atf_test_case format_numeric
format_numeric_head() {
	atf_set descr "numeric conversions produce numbers"
}
format_numeric_body() {
	atf_check -s exit:0 \
	    -e match:"^U=[0-9]+[.,][0-9]{2} S=[0-9]+[.,][0-9]{2} P=[0-9?]+%$" \
	    ${TIME} -f "U=%U S=%S P=%P" true
	atf_check -s exit:0 \
	    -e match:"^M=[0-9]+ t=[0-9]+ K=[0-9]+ D=[0-9]+ p=[0-9]+ X=[0-9]+$" \
	    ${TIME} -f "M=%M t=%t K=%K D=%D p=%p X=%X" true
	atf_check -s exit:0 \
	    -e match:"^F=[0-9]+ R=[0-9]+ W=[0-9]+ c=[0-9]+ w=[0-9]+$" \
	    ${TIME} -f "F=%F R=%R W=%W c=%c w=%w" true
	atf_check -s exit:0 \
	    -e match:"^I=[0-9]+ O=[0-9]+ r=[0-9]+ s=[0-9]+ k=[0-9]+$" \
	    ${TIME} -f "I=%I O=%O r=%r s=%s k=%k" true
}

atf_test_case format_pagesize
format_pagesize_head() {
	atf_set descr "%Z matches the system page size"
}
format_pagesize_body() {
	pagesize=$(sysctl -n hw.pagesize)
	atf_check -s exit:0 -e inline:"${pagesize}\n" ${TIME} -f "%Z" true
}

atf_test_case format_overrides
format_overrides_head() {
	atf_set descr "-f takes precedence over -p and -h, -l still appends"
}
format_overrides_body() {
	atf_check -s exit:0 -e inline:"fmt\n" ${TIME} -p -f "fmt" true
	atf_check -s exit:0 -e inline:"fmt\n" ${TIME} -h -f "fmt" true
	atf_check -s exit:0 -e match:"^fmt$" \
	    -e match:"maximum resident set size" \
	    ${TIME} -l -f "fmt" true
}

atf_test_case format_output_file
format_output_file_head() {
	atf_set descr "-f output honours -o and -a"
}
format_output_file_body() {
	atf_check -s exit:0 ${TIME} -o out.txt -f "first %x" true
	atf_check -o inline:"first 0\n" cat out.txt
	atf_check -s exit:0 ${TIME} -a -o out.txt -f "second %x" true
	atf_check -o inline:"first 0\nsecond 0\n" cat out.txt
	atf_check -s exit:0 ${TIME} -o out.txt -f "third %x" true
	atf_check -o inline:"third 0\n" cat out.txt
}

atf_test_case format_missing_arg
format_missing_arg_head() {
	atf_set descr "-f without an argument is a usage error"
}
format_missing_arg_body() {
	atf_check -s exit:1 -e match:"usage:" ${TIME} -f
}

atf_init_test_cases() {
	atf_add_test_case format_literal
	atf_add_test_case format_percent
	atf_add_test_case format_backslash
	atf_add_test_case format_unknown
	atf_add_test_case format_trailing
	atf_add_test_case format_command
	atf_add_test_case format_exit_status
	atf_add_test_case format_signal
	atf_add_test_case format_elapsed
	atf_add_test_case format_numeric
	atf_add_test_case format_pagesize
	atf_add_test_case format_overrides
	atf_add_test_case format_output_file
	atf_add_test_case format_missing_arg
}
