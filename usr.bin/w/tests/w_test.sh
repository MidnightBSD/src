#
# Basic regression tests for w(1) and uptime(1).
#

atf_test_case uptime_runs
uptime_runs_body()
{
	atf_check -s exit:0 -o not-empty uptime
}

atf_test_case w_runs
w_runs_body()
{
	atf_check -s exit:0 -o not-empty w
}

atf_init_test_cases()
{
	atf_add_test_case uptime_runs
	atf_add_test_case w_runs
}

