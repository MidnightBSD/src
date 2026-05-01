#
# Tests for date(1) -v adjustments
#

TEST1=3222243		# 1970-02-07 07:04:03 UTC

atf_test_case vary_seconds
vary_seconds_body()
{
	atf_check -o "inline:${TEST1}\n" env TZ=UTC0 date -r ${TEST1} +%s
	atf_check -o "inline:$((TEST1 + 1))\n" env TZ=UTC0 date -r ${TEST1} -v+1S +%s
	atf_check -o "inline:$((TEST1 - 1))\n" env TZ=UTC0 date -r ${TEST1} -v-1S +%s
}

atf_test_case vary_days
vary_days_body()
{
	atf_check -o "inline:$((TEST1 + 86400))\n" env TZ=UTC0 date -r ${TEST1} -v+1d +%s
	atf_check -o "inline:$((TEST1 - 86400))\n" env TZ=UTC0 date -r ${TEST1} -v-1d +%s
}

atf_test_case vary_overflow_rejected
vary_overflow_rejected_body()
{
	atf_check -s exit:1 -e match:".*Cannot apply date adjustment.*" \
	    env TZ=UTC0 date -r ${TEST1} -v+999999999999999999999S +%s
}

atf_init_test_cases()
{
	atf_add_test_case vary_seconds
	atf_add_test_case vary_days
	atf_add_test_case vary_overflow_rejected
}

