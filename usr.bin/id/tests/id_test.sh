#
# Basic regression tests for id(1), groups(1), and whoami(1).
#

atf_test_case id_basic
id_basic_body()
{
	atf_check -s exit:0 -o match:".*uid=.* gid=.*" id
	atf_check -s exit:0 -o match:"^[0-9]+$" id -u
	atf_check -s exit:0 -o match:"^[0-9]+$" id -g
	atf_check -s exit:0 -o not-empty id -G
	atf_check -s exit:0 -o not-empty id -G -n
}

atf_test_case whoami_matches_id
whoami_matches_id_body()
{
	u1="$(whoami)"
	u2="$(id -un)"
	atf_check_equal "${u1}" "${u2}"
}

atf_test_case groups_nonempty
groups_nonempty_body()
{
	atf_check -s exit:0 -o not-empty groups
}

atf_init_test_cases()
{
	atf_add_test_case id_basic
	atf_add_test_case whoami_matches_id
	atf_add_test_case groups_nonempty
}

