#
# Tests for date(1) input parsing without setting the clock (-j)
#

TEST1=3222243		# 1970-02-07 07:04:03 UTC

atf_test_case parse_numeric_with_seconds
parse_numeric_with_seconds_body()
{
	atf_check -o "inline:${TEST1}\n" \
	    env TZ=UTC0 date -j -f "%Y%m%d%H%M.%S" "197002070704.03" +%s
}

atf_test_case parse_numeric_without_seconds
parse_numeric_without_seconds_body()
{
	# Seconds default to 0 when not provided.
	atf_check -o "inline:3222240\n" \
	    env TZ=UTC0 date -j 197002070704 +%s
}

atf_test_case parse_extraneous_characters_warn
parse_extraneous_characters_warn_body()
{
	# strptime(3) should parse the valid prefix and warn about the rest.
	atf_check -s exit:0 -o "inline:${TEST1}\n" \
	    -e match:"Ignoring [0-9]+ extraneous characters in date string \\(Z\\)" \
	    env TZ=UTC0 date -j -f "%Y%m%d%H%M.%S" "197002070704.03Z" +%s
}

atf_test_case parse_invalid_format_rejected
parse_invalid_format_rejected_body()
{
	atf_check -s exit:1 -e match:".*illegal time format.*" \
	    env TZ=UTC0 date -j 1970020707 +%s
}

atf_init_test_cases()
{
	atf_add_test_case parse_numeric_with_seconds
	atf_add_test_case parse_numeric_without_seconds
	atf_add_test_case parse_extraneous_characters_warn
	atf_add_test_case parse_invalid_format_rejected
}
