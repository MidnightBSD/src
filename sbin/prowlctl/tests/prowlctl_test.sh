#
# Copyright (c) 2026 Lucas Holt
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

# These tests start a private prowld with its run directory and its unit
# directory inside the test's working directory (prowld -s DIR -u DIR).
# Paths are relative: a unix socket path is limited to 103 bytes, which a
# deep working directory can exceed.  That prowld is not init and does not read /etc/prowld or /etc/rc.d, so it
# never touches the running system.  They only start /bin/sleep and
# /usr/bin/true and false.

PROWLD=${PROWLD:-/sbin/prowld}
PROWLCTL=${PROWLCTL:-/sbin/prowlctl}
# for atf_check, which cannot run shell functions
CTL="${PROWLCTL} -s run"

# start_prowld: run a private prowld on the units already in ./units
start_prowld()
{
	mkdir -p run units
	"${PROWLD}" -d -s run -u units \
	    >prowld.log 2>&1 &
	echo $! >prowld.pid
	for i in $(jot 50); do
		[ -S run/prowld.sock ] && return 0
		sleep 0.2
	done
	cat prowld.log
	atf_fail "prowld did not create its socket"
}

# ctl args...: prowlctl against the private prowld
ctl()
{
	"${PROWLCTL}" -s run "$@"
}

# stop_prowld: shut the private prowld down; used from every cleanup
stop_prowld()
{
	[ -f prowld.pid ] || return 0
	pid=$(cat prowld.pid)
	if kill -0 "${pid}" 2>/dev/null; then
		kill -TERM "${pid}" 2>/dev/null
		for i in $(jot 50); do
			kill -0 "${pid}" 2>/dev/null || break
			sleep 0.2
		done
		kill -KILL "${pid}" 2>/dev/null
	fi
	return 0
}

# wait_state label state: wait until the job is in the given state
wait_state()
{
	for i in $(jot 75); do
		ctl status "$1" 2>/dev/null | grep -q "State: $2" && return 0
		sleep 0.2
	done
	ctl status "$1" || true
	cat prowld.log
	atf_fail "$1 did not reach state $2"
}

# job_pid label: print the job's pid
job_pid()
{
	ctl status "$1" | awk '/PID:/ { print $2 }'
}

# write_units: a daemon, a oneshot that succeeds and one that fails
write_units()
{
	mkdir -p units
	cat >units/svc.unit <<EOF
{"label": "org.test.svc", "rc_name": "prowlctl_test_svc",
 "type": "daemon", "program": "/bin/sleep",
 "arguments": ["300"], "run_at_load": true, "throttle_interval": 1}
EOF
	cat >units/once.unit <<EOF
{"label": "org.test.once", "rc_name": "prowlctl_test_once",
 "type": "oneshot", "program": "/usr/bin/true",
 "run_at_load": true}
EOF
	cat >units/bad.unit <<EOF
{"label": "org.test.bad", "rc_name": "prowlctl_test_bad",
 "type": "oneshot", "program": "/usr/bin/false",
 "run_at_load": true}
EOF
}

setup_common()
{
	atf_require_prog jot
	[ -x "${PROWLD}" ] || atf_skip "${PROWLD} is not installed"
	[ -x "${PROWLCTL}" ] || atf_skip "${PROWLCTL} is not installed"
	write_units
	start_prowld
	wait_state org.test.svc running
	wait_state org.test.once completed
	wait_state org.test.bad failed
}

atf_test_case list_type cleanup
list_type_head()
{
	atf_set descr "list --type filters on the server by job type"
	atf_set require.user root
}
list_type_body()
{
	setup_common
	ctl list --type=daemon >out
	atf_check -s exit:0 grep -q org.test.svc out
	atf_check -s not-exit:0 grep -q org.test.once out
	atf_check -s not-exit:0 grep -q org.test.bad out

	ctl list --type=oneshot >out
	atf_check -s exit:0 grep -q org.test.once out
	atf_check -s exit:0 grep -q org.test.bad out
	atf_check -s not-exit:0 grep -q org.test.svc out

	# An unknown type is an error, not an empty list.
	atf_check -s not-exit:0 -e match:"unknown job type" -o ignore \
	    ${CTL} list --type=bogus
}
list_type_cleanup()
{
	stop_prowld
}

atf_test_case list_failed cleanup
list_failed_head()
{
	atf_set descr "list --failed shows only failed jobs"
	atf_set require.user root
}
list_failed_body()
{
	setup_common
	ctl list --failed >out
	atf_check -s exit:0 grep -q org.test.bad out
	atf_check -s not-exit:0 grep -q org.test.svc out
	atf_check -s not-exit:0 grep -q org.test.once out

	# Filters combine: the failed job is a oneshot, not a daemon.
	ctl list --failed --type=daemon >out
	atf_check -s not-exit:0 grep -q org.test.bad out
	ctl list --failed --type=oneshot >out
	atf_check -s exit:0 grep -q org.test.bad out
}
list_failed_cleanup()
{
	stop_prowld
}

atf_test_case list_enabled cleanup
list_enabled_head()
{
	atf_set descr "list --enabled omits masked jobs"
	atf_set require.user root
}
list_enabled_body()
{
	setup_common
	ctl list --enabled >out
	atf_check -s exit:0 grep -q org.test.svc out
	atf_check -s exit:0 grep -q org.test.once out

	# Mask, not disable: disable runs sysrc(8) and would edit the real
	# /etc/rc.conf.  Masking only touches the private run directory.
	atf_check -o ignore ${CTL} mask org.test.svc
	ctl list --enabled >out
	atf_check -s not-exit:0 grep -q org.test.svc out
	atf_check -s exit:0 grep -q org.test.once out
}
list_enabled_cleanup()
{
	stop_prowld
}

atf_test_case stop_restart cleanup
stop_restart_head()
{
	atf_set descr "stop and restart act on the job's process"
	atf_set require.user root
}
stop_restart_body()
{
	setup_common
	old=$(job_pid org.test.svc)
	atf_check -o ignore kill -0 "${old}"

	atf_check -o ignore ${CTL} stop org.test.svc
	wait_state org.test.svc stopped
	atf_check -s not-exit:0 -e ignore kill -0 "${old}"

	atf_check -o ignore ${CTL} start org.test.svc
	wait_state org.test.svc running

	old=$(job_pid org.test.svc)
	atf_check -o ignore ${CTL} restart org.test.svc
	wait_state org.test.svc running
	new=$(job_pid org.test.svc)
	[ "${new}" != "${old}" ] || atf_fail "restart kept pid ${old}"
	atf_check -s not-exit:0 -e ignore kill -0 "${old}"
}
stop_restart_cleanup()
{
	stop_prowld
}

atf_test_case rc_verbs cleanup
rc_verbs_head()
{
	atf_set descr "rc.subr verb variants are accepted by prowlctl"
	atf_set require.user root
}
rc_verbs_body()
{
	setup_common
	atf_check -o match:"State: running" ${CTL} faststatus org.test.svc
	atf_check -o match:"State: running" ${CTL} onestatus org.test.svc
	atf_check -o ignore ${CTL} quietstop org.test.svc
	wait_state org.test.svc stopped
	atf_check -o ignore ${CTL} forcestart org.test.svc
	wait_state org.test.svc running
}
rc_verbs_cleanup()
{
	stop_prowld
}

atf_test_case show cleanup
show_head()
{
	atf_set descr "show prints the job as JSON"
	atf_set require.user root
}
show_body()
{
	setup_common
	atf_check -o match:'"label":"org.test.svc"' ${CTL} show org.test.svc
	atf_check -s not-exit:0 -e ignore -o ignore ${CTL} show org.test.nonexistent
}
show_cleanup()
{
	stop_prowld
}

atf_test_case shutdown cleanup
shutdown_head()
{
	atf_set descr "shutdown stops the jobs and waits for prowld to exit"
	atf_set require.user root
}
shutdown_body()
{
	setup_common
	pid=$(cat prowld.pid)
	svc=$(job_pid org.test.svc)

	atf_check -o ignore ${CTL} shutdown
	# prowlctl returns only once prowld is gone.
	atf_check -s not-exit:0 -e ignore kill -0 "${pid}"
	atf_check -s not-exit:0 -e ignore kill -0 "${svc}"
	[ ! -S run/prowld.sock ] || atf_fail "socket left behind"
}
shutdown_cleanup()
{
	stop_prowld
}

atf_test_case required_files cleanup
required_files_head()
{
	atf_set descr "a job does not start while a required file is missing"
	atf_set require.user root
}
required_files_body()
{
	req="$(pwd)/required.conf"
	mkdir -p units
	cat >units/req.unit <<EOF
{"label": "org.test.req", "rc_name": "prowlctl_test_req",
 "type": "daemon", "program": "/bin/sleep",
 "arguments": ["300"], "run_at_load": true,
 "required_files": ["${req}"]}
EOF
	# A relative path would weaken the check: the whole unit is refused.
	cat >units/rel.unit <<EOF
{"label": "org.test.rel", "rc_name": "prowlctl_test_rel",
 "type": "daemon", "program": "/bin/sleep",
 "arguments": ["300"], "run_at_load": true,
 "required_files": ["required.conf"]}
EOF
	start_prowld
	wait_state org.test.req failed
	atf_check -s not-exit:0 -e ignore -o ignore ${CTL} start org.test.req
	atf_check -s not-exit:0 -e ignore -o ignore ${CTL} status org.test.rel

	: >"${req}"
	atf_check -o ignore ${CTL} start org.test.req
	wait_state org.test.req running
}
required_files_cleanup()
{
	stop_prowld
}

atf_test_case stop_restart_wait cleanup
stop_restart_wait_head()
{
	atf_set descr "stopping a job that is waiting to restart keeps it stopped"
	atf_set require.user root
}
stop_restart_wait_body()
{
	mkdir -p units
	cat >units/flap.unit <<EOF
{"label": "org.test.flap", "rc_name": "prowlctl_test_flap",
 "type": "daemon", "program": "/bin/sh",
 "arguments": ["-c", "exit 1"], "run_at_load": true,
 "keep_alive": true, "throttle_interval": 3}
EOF
	start_prowld
	wait_state org.test.flap loaded
	atf_check -o ignore ${CTL} stop org.test.flap
	wait_state org.test.flap stopped
	# The cancelled restart must not bring it back.
	sleep 5
	atf_check -o match:'stopped' ${CTL} list
}
stop_restart_wait_cleanup()
{
	stop_prowld
}

atf_test_case oneshot_stop cleanup
oneshot_stop_head()
{
	atf_set descr "stopping a completed oneshot runs its stop command"
	atf_set require.user root
}
oneshot_stop_body()
{
	dir="$(pwd)"
	mkdir -p units
	cat >units/os.unit <<EOF
{"label": "org.test.os", "rc_name": "prowlctl_test_os",
 "type": "oneshot", "program": "/bin/sh",
 "arguments": ["-c", "echo start >> ${dir}/log"],
 "stop_arguments": ["-c", "echo stop >> ${dir}/log; exit 3"],
 "run_at_load": true}
EOF
	start_prowld
	wait_state org.test.os completed
	atf_check -o ignore ${CTL} stop org.test.os
	wait_state org.test.os stopped
	atf_check -o inline:"start\nstop\n" cat log
	# A stop command that fails still leaves the job stopped; starting
	# it again runs the start command once more.
	atf_check -o ignore ${CTL} start org.test.os
	wait_state org.test.os completed
	atf_check -o inline:"start\nstop\nstart\n" cat log
}
oneshot_stop_cleanup()
{
	stop_prowld
}

atf_test_case actions cleanup
actions_head()
{
	atf_set descr "reload, check and resync run the unit's commands"
	atf_set require.user root
}
actions_body()
{
	mkdir -p units
	cat >units/act.unit <<EOF
{"label": "org.test.act", "rc_name": "prowlctl_test_act",
 "type": "oneshot", "program": "/bin/sh",
 "arguments": ["-c", "exit 0"],
 "reload_arguments": ["-c", "echo reloaded; echo oops >&2"],
 "check_arguments": ["-c", "echo 'line \"one\"'; echo two; exit 3"],
 "resync_arguments": ["-c", "echo resynced"],
 "run_at_load": false}
EOF
	cat >units/plain.unit <<EOF
{"label": "org.test.plain", "rc_name": "prowlctl_test_plain",
 "type": "daemon", "program": "/bin/sleep",
 "arguments": ["300"], "run_at_load": true}
EOF
	start_prowld
	# check works before the job has been started, and reports its exit
	atf_check -s exit:3 -o inline:'line "one"\ntwo\n' ${CTL} check org.test.act
	# reload and resync need a started job
	atf_check -s not-exit:0 -e ignore -o ignore ${CTL} reload org.test.act
	atf_check -o ignore ${CTL} start org.test.act
	wait_state org.test.act completed
	atf_check -o match:reloaded -o match:oops ${CTL} reload org.test.act
	atf_check -o inline:'resynced\n' ${CTL} resync org.test.act
	# the rc.subr variants mean the same
	atf_check -o inline:'resynced\n' ${CTL} quietresync org.test.act
	# a unit without the command is refused; reload falls back to SIGHUP
	wait_state org.test.plain running
	atf_check -s not-exit:0 -e match:'no check command' -o ignore ${CTL} check org.test.plain
	atf_check -s not-exit:0 -e match:'no resync command' -o ignore ${CTL} resync org.test.plain
	atf_check -o inline:'ok\n' ${CTL} reload org.test.plain
}
actions_cleanup()
{
	stop_prowld
}

atf_test_case expansion cleanup
expansion_head()
{
	atf_set descr "@name@ in arguments and required_modules"
	atf_set require.user root
}
expansion_body()
{
	dir="$(pwd)"
	mkdir -p units
	cat >units/ex.unit <<EOF
{"label": "org.test.ex", "rc_name": "prowlctl_test_ex",
 "type": "oneshot", "program": "@nosuch_p:-/bin/sh@",
 "arguments": ["-c", "echo \"\$@\" > ${dir}/out", "sh",
   "@nosuch_a:-one two@", "x@nosuch_b:-y@z", "@@lit", "@nosuch_c@",
   "@nosuch_d?no@", "k@nosuch_e:-@"],
 "required_modules": ["nosuchfile:if_lo"],
 "run_at_load": true}
EOF
	cat >units/nomod.unit <<EOF
{"label": "org.test.nomod", "rc_name": "prowlctl_test_nomod",
 "type": "oneshot", "program": "/usr/bin/true",
 "required_modules": ["nosuchmodule_xyz"],
 "run_at_load": true}
EOF
	cat >units/nofile.unit <<EOF
{"label": "org.test.nofile", "rc_name": "prowlctl_test_nofile",
 "type": "oneshot", "program": "/usr/bin/true",
 "required_files": ["@nosuch_f:-/nonexistent/file@"],
 "run_at_load": true}
EOF
	start_prowld
	wait_state org.test.ex completed
	atf_check -o inline:"one two xyz @lit k\n" cat out
	# a module that cannot be loaded keeps the job from running
	wait_state org.test.nomod failed
	# required_files is expanded too
	wait_state org.test.nofile failed
}
expansion_cleanup()
{
	stop_prowld
}

atf_test_case forking cleanup
forking_head()
{
	atf_set descr "a daemon that forks away is found and supervised"
	atf_set require.user root
}
forking_body()
{
	dir="$(pwd)"
	mkdir -p units
	# Found by pidfile; stopped with SIGUSR1, as TERM is ignored.
	cat >units/fa.unit <<EOF
{"label": "org.test.fa", "rc_name": "prowlctl_test_fa",
 "type": "daemon", "program": "/bin/sh",
 "arguments": ["-c", "(trap 'echo usr1 >> ${dir}/log; exit 0' USR1; trap '' TERM; while :; do sleep 1; done) >/dev/null 2>&1 & echo \$! > ${dir}/a.pid"],
 "pid_file": "${dir}/a.pid", "stop_signal": "USR1",
 "run_at_load": true, "keep_alive": true, "throttle_interval": 1}
EOF
	# Found by name; comes back when it is killed.  A second process of
	# the same name is left behind by the first start and is not taken.
	cat >units/fb.unit <<EOF
{"label": "org.test.fb", "rc_name": "prowlctl_test_fb",
 "type": "daemon", "program": "/bin/sh",
 "arguments": ["-c", "sleep 300 >/dev/null 2>&1 & [ -f ${dir}/second ] || { touch ${dir}/second; sleep 400 >/dev/null 2>&1 & }"],
 "pid_name": "sleep",
 "run_at_load": true, "keep_alive": true, "throttle_interval": 1}
EOF
	# The daemon never shows up.
	cat >units/fn.unit <<EOF
{"label": "org.test.fn", "rc_name": "prowlctl_test_fn",
 "type": "daemon", "program": "/usr/bin/true",
 "pid_file": "${dir}/none.pid", "run_at_load": true}
EOF
	# A bad stop signal refuses the unit.
	cat >units/fx.unit <<EOF
{"label": "org.test.fx", "rc_name": "prowlctl_test_fx",
 "type": "daemon", "program": "/bin/sleep", "arguments": ["300"],
 "stop_signal": "NOSUCH", "run_at_load": true}
EOF
	start_prowld
	wait_state org.test.fa running
	pid=$(job_pid org.test.fa)
	atf_check_equal "${pid}" "$(cat a.pid)"
	atf_check_equal "$(cat prowld.pid)" "$(ps -o ppid= -p "${pid}" | tr -d ' ')"
	atf_check -o ignore ${CTL} stop org.test.fa
	wait_state org.test.fa stopped
	atf_check -o inline:"usr1\n" cat log
	atf_check -s exit:1 -e ignore kill -0 "${pid}"

	wait_state org.test.fb running
	pid=$(job_pid org.test.fb)
	atf_check_equal "sleep 300" "$(ps -o args= -p "${pid}")"
	kill -KILL "${pid}"
	new=
	for i in $(jot 75); do
		new=$(job_pid org.test.fb)
		[ "${new}" -gt 0 ] 2>/dev/null && [ "${new}" != "${pid}" ] && break
		sleep 0.2
	done
	wait_state org.test.fb running
	atf_check_not_equal "${pid}" "$(job_pid org.test.fb)"
	pid=$(job_pid org.test.fb)
	atf_check_equal "sleep 300" "$(ps -o args= -p "${pid}")"
	atf_check -o ignore ${CTL} stop org.test.fb
	wait_state org.test.fb stopped
	atf_check -s exit:1 -e ignore kill -0 "${pid}"

	wait_state org.test.fn failed
	atf_check -s not-exit:0 -e ignore -o ignore ${CTL} status org.test.fx
}
forking_cleanup()
{
	stop_prowld
	pkill -KILL -f "sleep (300|400)" 2>/dev/null || true
}

atf_test_case pre_post cleanup
pre_post_head()
{
	atf_set descr "start_pre runs before the job, stop_post after it"
	atf_set require.user root
}
pre_post_body()
{
	dir="$(pwd)"
	mkdir -p units
	cat >units/pp.unit <<EOF
{"label": "org.test.pp", "rc_name": "prowlctl_test_pp",
 "type": "daemon", "program": "/bin/sh",
 "arguments": ["-c", "echo main >> ${dir}/log; exec sleep 300"],
 "start_pre": [["/bin/sh", "-c", "echo pre1 >> ${dir}/log; sleep 2"],
               ["-/usr/bin/false"],
               ["/bin/sh", "-c", "echo @nosuch_pp:-pre2@ >> ${dir}/log"]],
 "stop_post": [["/bin/sh", "-c", "echo post1 >> ${dir}/log"],
               ["/usr/bin/false"],
               ["/bin/sh", "-c", "echo post2 >> ${dir}/log"]],
 "run_at_load": true, "keep_alive": true}
EOF
	# A failing start_pre keeps the job from running; stop_post still runs.
	cat >units/pf.unit <<EOF
{"label": "org.test.pf", "rc_name": "prowlctl_test_pf",
 "type": "daemon", "program": "/bin/sh",
 "arguments": ["-c", "echo pfmain >> ${dir}/log2; exec sleep 300"],
 "start_pre": [["/usr/bin/false"]],
 "run_at_load": true}
EOF
	# A start_pre that is not a list of commands refuses the unit.
	cat >units/px.unit <<EOF
{"label": "org.test.px", "rc_name": "prowlctl_test_px",
 "type": "daemon", "program": "/bin/sleep", "arguments": ["300"],
 "start_pre": [["relative"]], "run_at_load": true}
EOF
	start_prowld
	# Up only once the pre commands are done (they take 2 s).
	sleep 1
	atf_check -o match:'State: starting' ${CTL} status org.test.pp
	wait_state org.test.pp running
	atf_check -o inline:"pre1\npre2\nmain\n" cat log
	atf_check -s exit:1 -e ignore -o ignore test -f log2
	atf_check -o ignore ${CTL} stop org.test.pp
	wait_state org.test.pp stopped
	atf_check -o inline:"pre1\npre2\nmain\npost1\npost2\n" cat log
	wait_state org.test.pf failed
	[ ! -f log2 ] || atf_fail "pfmain ran after a failed start_pre"
	atf_check -s not-exit:0 -e ignore -o ignore ${CTL} status org.test.px
}
pre_post_cleanup()
{
	stop_prowld
	pkill -KILL -f "sleep 300" 2>/dev/null || true
}

atf_test_case activates cleanup
activates_head()
{
	atf_set descr "activates starts a disabled service first"
	atf_set require.user root
}
activates_body()
{
	mkdir -p units
	for n in b1 b2; do
		cat >units/${n}.unit <<EOF
{"label": "org.test.${n}", "rc_name": "prowlctl_test_${n}",
 "type": "daemon", "program": "/bin/sleep", "arguments": ["300"],
 "provides": ["act_${n}"], "run_at_load": false}
EOF
	done
	cat >units/u1.unit <<EOF
{"label": "org.test.u1", "rc_name": "prowlctl_test_u1",
 "type": "daemon", "program": "/bin/sleep", "arguments": ["300"],
 "activates": ["act_b1"], "run_at_load": true}
EOF
	cat >units/u2.unit <<EOF
{"label": "org.test.u2", "rc_name": "prowlctl_test_u2",
 "type": "daemon", "program": "/bin/sleep", "arguments": ["300"],
 "activates": ["act_b2"], "run_at_load": false}
EOF
	cat >units/u3.unit <<EOF
{"label": "org.test.u3", "rc_name": "prowlctl_test_u3",
 "type": "daemon", "program": "/bin/sleep", "arguments": ["300"],
 "activates": ["act_nosuch"], "run_at_load": true}
EOF
	start_prowld
	# At boot: the disabled service is started, then the one that needs it.
	wait_state org.test.b1 running
	wait_state org.test.u1 running
	# By hand: "start" brings up the activated service too.
	atf_check -o match:'State: (disabled|loaded)' ${CTL} status org.test.b2
	atf_check -o ignore ${CTL} start org.test.u2
	wait_state org.test.b2 running
	wait_state org.test.u2 running
	# Nothing provides it: the job waits.
	atf_check -o match:'State: loaded' ${CTL} status org.test.u3
}
activates_cleanup()
{
	stop_prowld
	pkill -KILL -f "sleep 300" 2>/dev/null || true
}

atf_test_case conditions cleanup
conditions_head()
{
	atf_set descr "conditions in front of a start_pre or stop_post command"
	atf_set require.user root
}
conditions_body()
{
	dir="$(pwd)"
	mkdir -p units /etc/rc.conf.d
	cat >/etc/rc.conf.d/prowlctl_test_cond <<EOF
cond_yes="YES"
cond_no="NO"
cond_val="x"
EOF
	cat >units/cd.unit <<EOF
{"label": "org.test.cd", "rc_name": "prowlctl_test_cond",
 "type": "daemon", "program": "/bin/sleep", "arguments": ["300"],
 "start_pre": [
   ["?cond_yes", "/bin/sh", "-c", "echo a >> ${dir}/log"],
   ["?cond_no", "/bin/sh", "-c", "echo b >> ${dir}/log"],
   ["!cond_no", "/bin/sh", "-c", "echo c >> ${dir}/log"],
   ["!cond_yes", "/bin/sh", "-c", "echo d >> ${dir}/log"],
   ["+cond_val", "?cond_yes", "!nosuch_cd", "/bin/sh", "-c", "echo e >> ${dir}/log"],
   ["+nosuch_cd", "/bin/sh", "-c", "echo f >> ${dir}/log"]],
 "stop_post": [
   ["?cond_no", "/bin/sh", "-c", "echo g >> ${dir}/log"],
   ["!cond_no", "/bin/sh", "-c", "echo h >> ${dir}/log"]],
 "run_at_load": true}
EOF
	cat >units/ce.unit <<EOF
{"label": "org.test.ce", "rc_name": "prowlctl_test_cond",
 "type": "daemon", "program": "/bin/sleep", "arguments": ["300"],
 "start_pre": [["?", "/bin/true"]], "run_at_load": true}
EOF
	cat >units/cf.unit <<EOF
{"label": "org.test.cf", "rc_name": "prowlctl_test_cond",
 "type": "daemon", "program": "/bin/sleep", "arguments": ["300"],
 "start_pre": [["?cond_yes"]], "run_at_load": true}
EOF
	start_prowld
	wait_state org.test.cd running
	atf_check -o inline:"a\nc\ne\n" cat log
	atf_check -o ignore ${CTL} stop org.test.cd
	wait_state org.test.cd stopped
	atf_check -o inline:"a\nc\ne\nh\n" cat log
	# An empty condition, or a command that is only conditions, is refused.
	atf_check -s not-exit:0 -e ignore -o ignore ${CTL} status org.test.ce
	atf_check -s not-exit:0 -e ignore -o ignore ${CTL} status org.test.cf
}
conditions_cleanup()
{
	stop_prowld
	rm -f /etc/rc.conf.d/prowlctl_test_cond
	pkill -KILL -f "sleep 300" 2>/dev/null || true
}

atf_test_case templates cleanup
templates_head()
{
	atf_set descr "template units are instantiated on demand"
	atf_set require.user root
}
templates_body()
{
	dir="$(pwd)"
	mkdir -p units
	cat >units/tmpl@.unit <<EOF
{"label": "org.test.tmpl@%i", "rc_name": "prowlctl_test_tmpl",
 "type": "daemon", "program": "/bin/sh",
 "arguments": ["-c", "echo %i > ${dir}/inst.%i; exec sleep 300"],
 "run_at_load": true, "default_instance": "one"}
EOF
	start_prowld
	# The default instance exists and runs.
	wait_state org.test.tmpl@one running
	test -f inst.one || atf_fail "%i was not substituted in instance one"

	# Others are created by name, with or without the label prefix.
	atf_check -o ignore ${CTL} start tmpl@two
	wait_state org.test.tmpl@two running
	atf_check -o inline:"two\n" cat inst.two
	atf_check -o match:'org.test.tmpl@two' ${CTL} show tmpl@two
	atf_check -o ignore ${CTL} stop tmpl@two
	wait_state org.test.tmpl@two stopped

	# Bad instance names and unknown templates are refused.
	atf_check -s not-exit:0 -e ignore -o ignore ${CTL} start tmpl@bad/x
	atf_check -s not-exit:0 -e ignore -o ignore ${CTL} start tmpl@..x
	atf_check -s not-exit:0 -e ignore -o ignore ${CTL} start nosuch@x
}
templates_cleanup()
{
	stop_prowld
}

atf_init_test_cases()
{
	atf_add_test_case conditions
	atf_add_test_case activates
	atf_add_test_case pre_post
	atf_add_test_case forking
	atf_add_test_case expansion
	atf_add_test_case actions
	atf_add_test_case oneshot_stop
	atf_add_test_case stop_restart_wait
	atf_add_test_case templates
	atf_add_test_case required_files
	atf_add_test_case list_type
	atf_add_test_case list_failed
	atf_add_test_case list_enabled
	atf_add_test_case stop_restart
	atf_add_test_case rc_verbs
	atf_add_test_case show
	atf_add_test_case shutdown
}
