#
# Basic regression tests for msearch(1) argument handling.
#

atf_test_case usage_no_args
usage_no_args_body()
{
	MSEARCH_BIN="$(ls /usr/obj/usr/src/*/usr.bin/msearch/msearch 2>/dev/null | head -n 1)"
	if [ -z "${MSEARCH_BIN}" ] || [ ! -x "${MSEARCH_BIN}" ]; then
		MSEARCH_BIN="msearch"
	fi
	atf_check -s exit:1 -e match:"^usage: msearch" "${MSEARCH_BIN}"
}

atf_test_case invalid_limit_rejected
invalid_limit_rejected_body()
{
	MSEARCH_BIN="$(ls /usr/obj/usr/src/*/usr.bin/msearch/msearch 2>/dev/null | head -n 1)"
	if [ -z "${MSEARCH_BIN}" ] || [ ! -x "${MSEARCH_BIN}" ]; then
		MSEARCH_BIN="msearch"
	fi
	atf_check -s exit:1 -e match:"invalid limit" "${MSEARCH_BIN}" -l notanint foo
	atf_check -s exit:1 -e match:"invalid limit" "${MSEARCH_BIN}" -l -1 foo
	atf_check -s exit:1 -e match:"invalid limit" "${MSEARCH_BIN}" -l 999999999999999999999 foo
}

atf_init_test_cases()
{
	atf_add_test_case usage_no_args
	atf_add_test_case invalid_limit_rejected
}
