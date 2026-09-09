#
# SPDX-License-Identifier: BSD-2-Clause
#

atf_test_case command
command_head()
{
	atf_set "descr" "run a command while holding a lock"
}
command_body()
{
	atf_check -s exit:0 -e empty -o inline:"locked\n" \
	    flock lock printf "locked\n"
	atf_check -s exit:0 -e empty -o empty test -f lock
}

atf_test_case shell_command
shell_command_head()
{
	atf_set "descr" "run an explicit shell command"
}
shell_command_body()
{
	atf_check -s exit:0 -e empty -o inline:"shell command\n" \
	    flock lock -c "printf 'shell command\\n'"
}

atf_test_case exit_status
exit_status_head()
{
	atf_set "descr" "preserve the executed command exit status"
}
exit_status_body()
{
	atf_check -s exit:7 -e empty -o empty flock lock sh -c "exit 7"
}

atf_test_case nonblocking
nonblocking_head()
{
	atf_set "descr" "fail rather than block when a lock is held"
}
nonblocking_cleanup()
{
	if [ -f holder.pid ]; then
		kill "$(cat holder.pid)" 2>/dev/null || true
	fi
}
nonblocking_body()
{
	flock lock tail -f /dev/null &
	holder=$!
	printf "%s\n" "${holder}" >holder.pid
	held=false
	for attempt in 1 2 3 4 5; do
		if ! flock -n lock true; then
			held=true
			break
		fi
		sleep 1
	done
	if [ "${held}" != true ]; then
		atf_fail "lock holder did not start"
	fi

	atf_check -s exit:1 -e empty -o empty flock -n lock true
	kill "${holder}" 2>/dev/null || true
	wait "${holder}" 2>/dev/null || true
	rm holder.pid
}

atf_test_case invalid_timeout
invalid_timeout_head()
{
	atf_set "descr" "reject malformed and negative timeout values"
}
invalid_timeout_body()
{
	atf_check -s exit:1 -e match:"invalid timeout" -o empty \
	    flock -w invalid lock true
	atf_check -s exit:1 -e match:"invalid timeout" -o empty \
	    flock -w -1 lock true
}

atf_init_test_cases()
{
	atf_add_test_case command
	atf_add_test_case shell_command
	atf_add_test_case exit_status
	atf_add_test_case nonblocking
	atf_add_test_case invalid_timeout
}
