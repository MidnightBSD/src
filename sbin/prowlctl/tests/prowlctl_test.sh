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

atf_test_case action_deadline cleanup
action_deadline_head()
{
	atf_set descr "An action's deadline covers process exit after output closes"
	atf_set require.user root
	atf_set timeout 60
}
action_deadline_body()
{
	mkdir units
	cat >units/action.unit <<'EOF'
{"label":"org.test.action","type":"oneshot","program":"/bin/sh",
 "run_at_load":false,
 "check_arguments":["-c","echo partial; exec 1>&- 2>&-; : >checking; exec /bin/sleep 45"]}
EOF
	start_prowld
	ctl check org.test.action >out &
	client=$!
	for i in $(jot 50); do
		[ -f checking ] && break
		sleep 0.1
	done
	[ -f checking ] || atf_fail "action did not start"
	atf_check -s exit:0 -o ignore timeout 5 ${CTL} status org.test.action
	code=0
	wait "$client" || code=$?
	atf_check_equal 124 "$code"
	atf_check -o inline:'partial\n' cat out
}
action_deadline_cleanup()
{
	stop_prowld
}

atf_test_case mask_activation cleanup
mask_activation_head()
{
	atf_set descr "A mask prevents socket launches and preserves stop escalation"
	atf_set require.user root
	atf_set require.progs nc
}
mask_activation_body()
{
	mkdir units
	cat >units/socket.unit <<'EOF'
{"label":"org.test.listener","program":"/bin/sh","run_at_load":true,
 "arguments":["-c",": >activated; exec /bin/sleep 40"],
 "sockets":{"main":{"path":"listener.sock"}}}
EOF
	cat >units/stubborn.unit <<'EOF'
{"label":"org.test.stubborn","program":"/bin/sh","run_at_load":true,
 "exit_timeout":1,"keep_alive":true,
 "arguments":["-c","trap '' TERM; : >ready; exec /bin/sleep 40"]}
EOF
	start_prowld
	wait_state org.test.listener running
	wait_state org.test.stubborn running
	[ -f ready ] || atf_fail "TERM handler was not installed"
	pid=$(job_pid org.test.stubborn)
	atf_check -o ignore ${CTL} mask org.test.stubborn
	atf_check -o ignore ${CTL} mask org.test.listener
	[ -L run/db/masked.d/org.test.listener ] || atf_fail "missing private mask"
	atf_check -s not-exit:0 -o ignore -e ignore ${CTL} start org.test.listener
	: >empty
	atf_check -s not-exit:0 -o ignore -e ignore nc -w 1 -U listener.sock <empty
	for i in $(jot 50); do
		kill -0 "$pid" 2>/dev/null || break
		sleep 0.1
	done
	kill -0 "$pid" 2>/dev/null && atf_fail "masked process survived its stop deadline"
	wait_state org.test.stubborn masked
	[ ! -e activated ] || atf_fail "masked listener launched its job"
	atf_check -o ignore ${CTL} unmask org.test.listener
	wait_state org.test.listener running
	atf_check -s exit:0 -o ignore nc -w 1 -U listener.sock <empty
	for i in $(jot 50); do
		[ -f activated ] && break
		sleep 0.1
	done
	[ -f activated ] || atf_fail "unmasked listener did not activate"
}
mask_activation_cleanup()
{
	stop_prowld
}

atf_test_case persist_enabled cleanup
persist_enabled_head()
{
	atf_set descr "Enable and disable acknowledge completed sysrc writes in order"
	atf_set require.user root
	atf_set require.progs "chroot /rescue/sh"
}
persist_enabled_body()
{
	mkdir -p root/bin root/etc root/usr/sbin root/units root/run
	cp "${PROWLD}" root/prowld
	cp /rescue/sh root/bin/sh
	cat >root/units/service.unit <<'EOF'
{"label":"test.persist","type":"oneshot","program":"/bin/sh","run_at_load":false}
EOF
	chroot root /prowld -d -s /run -u /units >prowld.log 2>&1 &
	echo $! >prowld.pid
	for i in $(jot 100); do
		[ -S root/run/prowld.sock ] && break
		sleep 0.05
	done
	client="${PROWLCTL} -s root/run"
	atf_check -s not-exit:0 -e match:sysrc ${client} enable test.persist
	atf_check -o match:'Enabled: false' ${client} status test.persist

	cat >root/usr/sbin/sysrc <<'EOF'
#!/bin/sh
[ "$1" = "--" ] && shift
[ ! -e /reject ] || exit 1
if [ -e /signal ]; then kill -TERM $$; fi
if [ "$1" = persist_enable=YES ] && [ -p /gate ]; then
	: >/waiting
	read -r ready </gate || exit 1
fi
printf '%s\n' "$1" >>/writes && printf '%s\n' "$1" >/etc/rc.conf
EOF
	chmod 755 root/usr/sbin/sysrc
	mkfifo root/gate
	${client} enable test.persist >enable.out &
	enabling=$!
	for i in $(jot 100); do
		[ -e root/waiting ] && break
		sleep 0.05
	done
	[ -e root/waiting ] || atf_fail "sysrc did not start"
	atf_check -o match:'Enabled: false' timeout 5 ${client} status test.persist
	${client} disable test.persist >disable.out &
	disabling=$!
	echo ready >root/gate
	wait "$enabling" && wait "$disabling" || atf_fail "queued write failed"
	atf_check -o inline:'persist_enable=YES\npersist_enable=NO\n' cat root/writes
	atf_check -o match:'Enabled: false' ${client} status test.persist
	rm root/gate
	atf_check -o ignore ${client} enable test.persist
	: >root/reject
	atf_check -s not-exit:0 -e match:sysrc ${client} disable test.persist
	rm root/reject
	: >root/signal
	atf_check -s not-exit:0 -e match:sysrc ${client} disable test.persist
	atf_check -o match:'Enabled: true' ${client} status test.persist
	atf_check -o inline:'persist_enable=YES\n' cat root/etc/rc.conf
}
persist_enabled_cleanup()
{
	stop_prowld
}

atf_test_case masked_timer cleanup
masked_timer_head()
{
	atf_set descr "A saved mask prevents boot and interval timer launches"
	atf_set require.user root
	atf_set require.progs chroot
}
masked_timer_body()
{
	# Keep both the persistent masks and the timer state in the fixture.
	mkdir -p root/etc/prowld/masks.d root/var/db/prowld/masked.d \
	    root/var/run root/var/log root/units root/lib root/libexec root/dev
	: >root/dev/null
	cp "${PROWLD}" root/prowld
	cp /bin/echo root/echo
	cp /lib/libc.so.7 root/lib/
	cp /libexec/ld-elf.so.1 root/libexec/
	for timer in allowed protected legacy; do
		echo "${timer}_enable=YES" >>root/etc/rc.conf
		cat >"root/units/$timer.unit" <<EOF
{"label":"org.test.$timer","type":"timer","run_at_load":false,
 "schedule":{"interval":1,"on_boot_delay":1},"program":"/echo",
 "arguments":["tick"],"standard_out_path":"/$timer.out"}
EOF
	done
	new=root/etc/prowld/masks.d
	old=root/var/db/prowld/masked.d
	run=root/var/run/prowld
	ln -s /dev/null "$new/org.test.protected"
	ln -s /dev/null "$old/org.test.legacy"
	chroot root /prowld -d -u /units >prowld.log 2>&1 &
	echo $! >prowld.pid
	for i in $(jot 60); do
		[ -f root/allowed.out ] && \
		    [ "$(grep -c '^tick$' root/allowed.out)" -ge 2 ] && break
		sleep 0.1
	done
	[ -f root/allowed.out ] && [ "$(grep -c '^tick$' root/allowed.out)" -ge 2 ] ||
	    atf_fail "control timer did not fire twice"
	[ ! -e root/protected.out ] || atf_fail "masked timer ran"
	[ ! -e root/legacy.out ] || atf_fail "legacy-masked timer ran"
	atf_check -o match:'State: masked' "${PROWLCTL}" -s "$run" status org.test.protected
	atf_check -o ignore "${PROWLCTL}" -s "$run" mask org.test.legacy
	[ -L "$new/org.test.legacy" ] || atf_fail "missing early-boot mask"
	atf_check -o ignore "${PROWLCTL}" -s "$run" unmask org.test.legacy
	[ ! -L "$new/org.test.legacy" ] && [ ! -L "$old/org.test.legacy" ] ||
	    atf_fail "unmask left a saved mask"
	atf_check -o ignore "${PROWLCTL}" -s "$run" unmask org.test.protected
	for i in $(jot 50); do
		[ -s root/protected.out ] && break
		sleep 0.1
	done
	atf_check -o ignore grep -qx tick root/protected.out
}
masked_timer_cleanup()
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

atf_test_case forking_launch_identity cleanup
forking_launch_identity_head()
{
	atf_set descr "Concurrent double-forked daemons with the same name are tracked separately"
	atf_set require.user root
	atf_set timeout 60
}
forking_launch_identity_body()
{
	dir=$(pwd)
	mkdir units
	cp /bin/sh fork-worker
	cat >worker.sh <<'EOF'
echo "$$" >"$1/$2.pid"
for i in $(jot 60); do
	[ ! -e "$1/done" ] || exit 0
	sleep 1
done
EOF
	cat >launch.sh <<'EOF'
( "$1/fork-worker" "$1/worker.sh" "$1" "$2" & ) &
wait "$!" || exit 1
# Keep both launchers alive until both workers have started.
for i in $(jot 100); do
	[ -s "$1/first.pid" ] && [ -s "$1/second.pid" ] && exit 0
	sleep 0.1
done
exit 1
EOF
	for name in first second; do
		cat >"units/$name.unit" <<EOF
{"label":"org.test.$name","type":"daemon","program":"/bin/sh",
 "arguments":["$dir/launch.sh","$dir","$name"],
 "pid_name":"fork-worker","run_at_load":true}
EOF
	done
	start_prowld
	for name in first second; do
		wait_state "org.test.$name" running
		atf_check_equal "$(cat "$name.pid")" "$(job_pid "org.test.$name")"
	done
	atf_check -o ignore ${CTL} stop org.test.first
	wait_state org.test.first stopped
	atf_check -o ignore kill -0 "$(cat second.pid)"
	atf_check_equal "$(cat second.pid)" "$(job_pid org.test.second)"
	atf_check -o ignore ${CTL} stop org.test.second
	wait_state org.test.second stopped
}
forking_launch_identity_cleanup()
{
	touch done
	stop_prowld
}

atf_test_case forking_pidfile cleanup
forking_pidfile_head()
{
	atf_set descr "Pidfile adoption requires a regular file and a member of the job's own launch"
	atf_set require.user root
	atf_set require.progs "mkfifo timeout"
	atf_set timeout 60
}
forking_pidfile_body()
{
	dir=$(pwd)
	mkdir units
	cat >units/peer.unit <<'EOF'
{"label":"org.test.peer","type":"daemon","program":"/bin/sleep",
 "arguments":["60"],"run_at_load":true}
EOF
	cat >units/fifo.unit <<EOF
{"label":"org.test.fifo","type":"daemon","program":"/usr/bin/mkfifo",
 "arguments":["$dir/service.fifo"],"pid_file":"$dir/service.fifo"}
EOF
	cat >launcher.sh <<'EOF'
sleep 60 &
echo "$!" >"$1/worker.pid"
cp "$1/peer.pid" "$1/service.pid"
EOF
	cat >units/foreign.unit <<EOF
{"label":"org.test.foreign","type":"daemon","program":"/bin/sh",
 "arguments":["$dir/launcher.sh","$dir"],"pid_file":"$dir/service.pid"}
EOF
	start_prowld
	wait_state org.test.peer running
	peer=$(job_pid org.test.peer)
	echo "$peer" >peer.pid
	atf_check -o ignore ${CTL} start org.test.fifo
	for i in $(jot 50); do
		[ -p service.fifo ] && break
		sleep 0.1
	done
	[ -p service.fifo ] || atf_fail "pidfile fixture was not created"
	atf_check -o match:'State: running' timeout 5 ${CTL} status org.test.peer
	atf_check -o ignore timeout 5 ${CTL} start org.test.foreign
	wait_state org.test.fifo failed
	wait_state org.test.foreign failed
	atf_check_equal "$peer" "$(cat service.pid)"
	atf_check_equal "-1" "$(job_pid org.test.foreign)"
	atf_check -s not-exit:0 -e ignore kill -0 "$(cat worker.pid)"

	# Stop after the new launcher has written the foreign pidfile.
	rm service.pid
	atf_check -o ignore ${CTL} start org.test.foreign
	for i in $(jot 50); do
		[ -s service.pid ] && [ "$(job_pid org.test.foreign)" = -1 ] && break
		sleep 0.1
	done
	atf_check_equal "-1" "$(job_pid org.test.foreign)"
	atf_check -o ignore timeout 5 ${CTL} stop org.test.foreign
	wait_state org.test.foreign stopped
	atf_check -s not-exit:0 -e ignore kill -0 "$(cat worker.pid)"
	atf_check -o match:'State: running' ${CTL} status org.test.peer
	atf_check_equal "$peer" "$(job_pid org.test.peer)"
	atf_check -o ignore kill -0 "$peer"
}
forking_pidfile_cleanup()
{
	stop_prowld
}

atf_test_case shim_pidfile_status cleanup
shim_pidfile_status_head()
{
	atf_set descr "rc-shim status reads only regular pidfiles"
	atf_set require.user root
	atf_set require.progs "chroot mkfifo timeout"
}
shim_pidfile_status_body()
{
	mkdir -p root/etc/rc.d root/run
	cp "${PROWLD}" root/prowld
	cat >root/etc/rc.d/pidprobe <<'EOF'
#!/bin/sh
# PROVIDE: pidprobe
name=pidprobe
rcvar=pidprobe_enable
pidfile=/run/pidprobe.pid
EOF
	chmod 755 root/etc/rc.d/pidprobe
	mkfifo root/run/pidprobe.pid
	chroot root /prowld -d -s /run >prowld.log 2>&1 &
	echo $! >prowld.pid
	for i in $(jot 50); do
		[ -S root/run/prowld.sock ] && break
		sleep 0.2
	done
	atf_check -o match:'PID: -1$' timeout 5 "${PROWLCTL}" -s root/run status pidprobe
	rm root/run/pidprobe.pid
	self=$$
	printf '%s\ncommand arguments\n' "$self" >root/run/pidprobe.pid
	atf_check -o match:"PID: ${self}$" "${PROWLCTL}" -s root/run status pidprobe
}
shim_pidfile_status_cleanup()
{
	stop_prowld
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
	# Enabled in rc.conf but not started at boot: it is started too.
	mkdir -p /etc/rc.conf.d
	echo 'prowlctl_test_b3_enable="YES"' >/etc/rc.conf.d/prowlctl_test_b3
	cat >units/b3.unit <<EOF
{"label": "org.test.b3", "rc_name": "prowlctl_test_b3",
 "type": "daemon", "program": "/bin/sleep", "arguments": ["300"],
 "provides": ["act_b3"], "run_at_load": false}
EOF
	cat >units/u4.unit <<EOF
{"label": "org.test.u4", "rc_name": "prowlctl_test_u4",
 "type": "daemon", "program": "/bin/sleep", "arguments": ["300"],
 "activates": ["act_b3"], "run_at_load": true}
EOF
	start_prowld
	wait_state org.test.b3 running
	wait_state org.test.u4 running
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
	rm -f /etc/rc.conf.d/prowlctl_test_b3
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
	cat >units/cp.unit <<EOF
{"label": "org.test.cp", "rc_name": "prowlctl_test_cond",
 "type": "oneshot", "program": "/bin/sh",
 "arguments": ["-c", "echo \"\$@\" > ${dir}/out", "sh",
   "@cond_val:+-V@", "@cond_val@", "@nosuch_cp:+-W@", "@nosuch_cp@"],
 "run_at_load": true}
EOF
	for n in b1 b2; do
		cat >units/c${n}.unit <<EOF
{"label": "org.test.c${n}", "rc_name": "prowlctl_test_c${n}",
 "type": "daemon", "program": "/bin/sleep", "arguments": ["300"],
 "provides": ["cond_${n}"], "run_at_load": false}
EOF
	done
	cat >units/cu.unit <<EOF
{"label": "org.test.cu", "rc_name": "prowlctl_test_cond",
 "type": "daemon", "program": "/bin/sleep", "arguments": ["300"],
 "activates": ["?cond_no cond_b1", "?cond_yes cond_b2"],
 "requires": ["!cond_yes cond_nosuch"], "run_at_load": true}
EOF
	start_prowld
	wait_state org.test.cp completed
	atf_check -o inline:"-V x\n" cat out
	# Only the activation whose condition holds starts its service.
	wait_state org.test.cu running
	wait_state org.test.cb2 running
	atf_check -o match:'State: disabled' ${CTL} status org.test.cb1
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

	# "start tmpl two" means "start tmpl@two".
	atf_check -o ignore ${CTL} start tmpl two
	wait_state org.test.tmpl@two running
	atf_check -o ignore ${CTL} stop tmpl two
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

atf_test_case dependency_loop cleanup
dependency_loop_head()
{
	atf_set "descr" "A dependency loop does not fail the jobs in it"
	atf_set "require.user" "root"
}
dependency_loop_body()
{
	dir="$(pwd)"
	mkdir -p units
	# dl1 waits for dl2, and says it must come before dl2: a loop.  The
	# wait that only comes from "before" goes, so dl2 still runs first.
	cat >units/dl1.unit <<EOF
{"label": "org.test.dl1", "rc_name": "dl1", "type": "oneshot",
 "program": "/bin/sh", "arguments": ["-c", "echo dl1 >> ${dir}/log"],
 "wants": ["dl2"], "before": ["dl2"], "run_at_load": true}
EOF
	cat >units/dl2.unit <<EOF
{"label": "org.test.dl2", "rc_name": "dl2", "type": "oneshot",
 "program": "/bin/sh", "arguments": ["-c", "echo dl2 >> ${dir}/log"],
 "run_at_load": true}
EOF
	# dl3 waits for dl1, and dl4 and dl5 wait for each other.
	cat >units/dl3.unit <<EOF
{"label": "org.test.dl3", "rc_name": "dl3", "type": "oneshot",
 "program": "/usr/bin/true", "wants": ["dl1"], "run_at_load": true}
EOF
	cat >units/dl4.unit <<EOF
{"label": "org.test.dl4", "rc_name": "dl4", "type": "oneshot",
 "program": "/usr/bin/true", "wants": ["dl5"], "run_at_load": true}
EOF
	cat >units/dl5.unit <<EOF
{"label": "org.test.dl5", "rc_name": "dl5", "type": "oneshot",
 "program": "/usr/bin/true", "wants": ["dl4"], "run_at_load": true}
EOF
	start_prowld
	for n in 1 2 3 4 5; do
		wait_state org.test.dl$n completed
	done
	atf_check -o inline:"dl2\ndl1\n" cat log
}
dependency_loop_cleanup()
{
	stop_prowld
}

atf_test_case oneshot_lists cleanup
oneshot_lists_head()
{
	atf_set descr "start_post and stop_pre, and the ~ and < conditions"
	atf_set require.user root
}
oneshot_lists_body()
{
	dir="$(pwd)"
	mkdir -p units /etc/rc.conf.d
	: > present
	cat >/etc/rc.conf.d/prowlctl_test_ol <<EOF
ol_empty=""
ol_val="x"
ol_here="${dir}/present"
ol_gone="${dir}/nothere"
EOF
	cat >units/ol.unit <<EOF
{"label": "org.test.ol", "rc_name": "prowlctl_test_ol",
 "type": "oneshot", "program": "/bin/sh",
 "arguments": ["-c", "echo main >> ${dir}/log"],
 "stop_arguments": ["-c", "echo stop >> ${dir}/log"],
 "start_pre": [["/bin/sh", "-c", "echo pre >> ${dir}/log"]],
 "start_post": [
   ["~ol_empty", "/bin/sh", "-c", "echo a >> ${dir}/log"],
   ["~ol_val", "/bin/sh", "-c", "echo b >> ${dir}/log"],
   ["<ol_here", "/bin/sh", "-c", "echo c >> ${dir}/log"],
   ["<ol_gone", "/bin/sh", "-c", "echo d >> ${dir}/log"],
   ["!<ol_gone", "/bin/sh", "-c", "echo e >> ${dir}/log"],
   ["!<ol_here", "/bin/sh", "-c", "echo f >> ${dir}/log"],
   ["<ol_empty", "/bin/sh", "-c", "echo g >> ${dir}/log"]],
 "stop_pre": [["/bin/sh", "-c", "echo prestop >> ${dir}/log"]],
 "run_at_load": true}
EOF
	cat >units/of.unit <<EOF
{"label": "org.test.of", "rc_name": "prowlctl_test_ol",
 "type": "oneshot", "program": "/usr/bin/true",
 "start_post": [["/bin/sh", "-c", "exit 1"]], "run_at_load": true}
EOF
	cat >units/oi.unit <<EOF
{"label": "org.test.oi", "rc_name": "prowlctl_test_ol",
 "type": "oneshot", "program": "/usr/bin/true",
 "start_post": [["-/bin/sh", "-c", "exit 1"]], "run_at_load": true}
EOF
	cat >units/od.unit <<EOF
{"label": "org.test.od", "rc_name": "prowlctl_test_ol",
 "type": "daemon", "program": "/bin/sleep", "arguments": ["300"],
 "start_post": [["/usr/bin/true"]], "run_at_load": true}
EOF
	start_prowld
	wait_state org.test.ol completed
	atf_check -o inline:"pre\nmain\na\nc\ne\n" cat log
	atf_check -o ignore ${CTL} stop org.test.ol
	wait_state org.test.ol stopped
	atf_check -o inline:"pre\nmain\na\nc\ne\nprestop\nstop\n" cat log
	# A start_post command that fails fails the job, unless it has a "-".
	wait_state org.test.of failed
	wait_state org.test.oi completed
	# start_post is for oneshot jobs.
	atf_check -s not-exit:0 -e ignore -o ignore ${CTL} status org.test.od
}
oneshot_lists_cleanup()
{
	rm -f /etc/rc.conf.d/prowlctl_test_ol
	stop_prowld
}

atf_test_case post_hook_timeout cleanup
post_hook_timeout_head()
{
	atf_set descr "A timed-out required start_post hook fails the oneshot"
	atf_set require.user root
	atf_set timeout 65
}
post_hook_timeout_body()
{
	mkdir units
	cat >units/slow.unit <<'EOF'
{"label":"org.test.slow","type":"oneshot","program":"/usr/bin/true",
 "run_at_load":false,"start_post":[["/bin/sleep","45"],
 ["/bin/sh","-c",": >hooks_done"]]}
EOF
	start_prowld
	atf_check -o ignore ${CTL} start org.test.slow
	for i in $(jot 40); do
		[ -f hooks_done ] && break
		sleep 1
	done
	[ -f hooks_done ] || atf_fail "remaining hook did not run after timeout"
	wait_state org.test.slow failed
}
post_hook_timeout_cleanup()
{
	stop_prowld
}

atf_test_case standard_streams cleanup
standard_streams_head()
{
	atf_set descr "A failed standard-stream redirect prevents execution"
	atf_set require.user root
}
standard_streams_body()
{
	mkdir units
	for stream in in out error; do
		cat >"units/$stream.unit" <<EOF
{"label":"org.test.$stream","type":"oneshot","program":"/bin/sh",
 "arguments":["-c",": >ran_$stream"],"run_at_load":true,
 "standard_${stream}_path":"$(pwd)/missing/file"}
EOF
	done
	start_prowld
	for stream in in out error; do
		wait_state "org.test.$stream" failed
		[ ! -e "ran_$stream" ] || atf_fail "command ran after bad $stream redirect"
	done
}
standard_streams_cleanup()
{
	stop_prowld
}

atf_test_case credentials cleanup
credentials_head()
{
	atf_set descr "Configured job credentials are applied or execution is refused"
	atf_set require.user root
	atf_set require.progs "chroot jot pwd_mkdb"
}
credentials_body()
{
	[ -x "${PROWLD}" ] || atf_skip "${PROWLD} is not installed"
	[ -x "${PROWLCTL}" ] || atf_skip "${PROWLCTL} is not installed"

	# Use a private account database so that group membership does not
	# depend on the test host and no host accounts need to be changed.
	mkdir -p root/etc root/lib root/libexec root/usr/bin root/units root/dev
	: >root/dev/null
	cp "${PROWLD}" root/prowld
	cp /usr/bin/id root/usr/bin/
	cp /libexec/ld-elf.so.1 root/libexec/
	cp /lib/libc.so.7 root/lib/
	cat >root/etc/master.passwd <<EOF
root:*:0:0::0:0:root:/:/bin/sh
service:*:101:101::0:0:service:/:/bin/sh
many:*:102:101::0:0:many:/:/bin/sh
EOF
	atf_check -o ignore pwd_mkdb -d root/etc root/etc/master.passwd
	cat >root/etc/nsswitch.conf <<EOF
passwd: files
group: files
EOF
	cat >root/etc/group <<EOF
wheel:*:0:
service:*:101:
EOF
	for gid in $(jot 32 200); do
		echo "extra${gid}:*:${gid}:many" >>root/etc/group
	done
	: >root/valid.out
	chown 101:101 root/valid.out

	for entry in 'missing_user "user": "absent"' \
	    'missing_group "group": "absent"' 'many "user": "many"' \
	    'group_only "group": "service"' 'valid "user": "service"'; do
		name=${entry%% *}
		identity=${entry#* }
		cat >"root/units/${name}.unit" <<EOF
{"label": "org.test.${name}", "type": "oneshot",
 "program": "/usr/bin/id", "arguments": ["-G"],
 "standard_out_path": "/${name}.out", "check_arguments": ["-u"],
 "run_at_load": true, ${identity}}
EOF
	done

	long_name=$(printf '%064d' 0)
	for field in user group; do
		for bad in empty number long nul duplicate; do
			case ${bad} in
			empty)	value='""' ;;
			number)	value=0 ;;
			long)	value="\"${long_name}\"" ;;
			nul)	value='"root\u0000service"' ;;
			duplicate) value="\"root\", \"${field}\": \"service\"" ;;
			esac
			name="${field}_${bad}"
			cat >"root/units/${name}.unit" <<EOF
{"label": "org.test.${name}", "type": "oneshot",
 "program": "/usr/bin/id", "standard_out_path": "/${name}.out",
 "run_at_load": true, "${field}": ${value}}
EOF
		done
	done

	# Seed a supplementary group that neither valid unit should inherit.
	chroot -G 999 root /prowld -d -s run -u units >prowld.log 2>&1 &
	echo $! >prowld.pid
	for i in $(jot 50); do
		[ -S root/run/prowld.sock ] && break
		sleep 0.2
	done
	[ -S root/run/prowld.sock ] || atf_fail "prowld did not create its socket"
	ctl_path="${PROWLCTL} -s root/run"

	for field in user group; do
		for bad in empty number long nul duplicate; do
			name="${field}_${bad}"
			atf_check -s not-exit:0 -o ignore -e ignore \
			    ${ctl_path} status "org.test.${name}"
			[ ! -e "root/${name}.out" ] || atf_fail "${name} was executed"
		done
	done
	for name in missing_user missing_group many; do
		atf_check -o match:'State: failed' ${ctl_path} status "org.test.${name}"
		atf_check -s not-exit:0 -o ignore -e ignore \
		    ${ctl_path} check "org.test.${name}"
		[ ! -e "root/${name}.out" ] || atf_fail "${name} was executed"
	done
	for name in group_only valid; do
		for i in $(jot 50); do
			${ctl_path} status "org.test.${name}" |
			    grep -q 'State: completed' && break
			sleep 0.2
		done
		atf_check -o match:'State: completed' ${ctl_path} status "org.test.${name}"
		atf_check -o inline:'101\n' cat "root/${name}.out"
	done
	atf_check -o inline:'0\n' ${ctl_path} check org.test.group_only
	atf_check -o inline:'101\n' ${ctl_path} check org.test.valid
}
credentials_cleanup()
{
	stop_prowld
}

atf_test_case interactive cleanup
interactive_head()
{
	atf_set descr "interactive is a boolean; outside PID 1 stdin stays /dev/null"
	atf_set require.user root
}
interactive_body()
{
	mkdir -p units
	# Not PID 1: the console is not offered, and the job runs as usual.
	cat >units/ia.unit <<EOF
{"label": "org.test.ia", "rc_name": "prowlctl_test_ia",
 "type": "daemon", "program": "/bin/sh",
 "arguments": ["-c", "if [ -t 0 ]; then exit 3; fi; exec sleep 300"],
 "run_at_load": true, "interactive": true}
EOF
	cat >units/bad.unit <<EOF
{"label": "org.test.iabad", "rc_name": "prowlctl_test_iabad",
 "type": "daemon", "program": "/bin/sleep",
 "arguments": ["300"], "run_at_load": true, "interactive": "yes"}
EOF
	start_prowld
	wait_state org.test.ia running
	atf_check -s not-exit:0 -e ignore -o ignore ${CTL} status org.test.iabad
}
interactive_cleanup()
{
	stop_prowld
}

atf_test_case unix_socket_access cleanup
unix_socket_access_head()
{
	atf_set descr "Unix socket activation preserves files and sets requested access"
	atf_set require.user root
}
unix_socket_access_body()
{
	mkdir units
	echo keep >occupied
	ln -s occupied linked
	for label in occupied linked restricted legacy; do
		access=
		if [ "$label" = restricted ]; then
			access=', "mode": "0660", "owner": "root", "group": "wheel"'
		fi
		cat >"units/$label.unit" <<EOF
{"label":"org.test.$label","run_at_load":true,"program":"/usr/bin/true",
 "sockets":{"main":{"path":"$label"$access}}}
EOF
	done
	start_prowld
	wait_state org.test.occupied failed
	wait_state org.test.linked failed
	atf_check -o inline:'keep\n' cat occupied
	atf_check -o inline:'occupied\n' readlink linked
	wait_state org.test.restricted running
	wait_state org.test.legacy running
	[ -S legacy ] || atf_fail "missing legacy socket"
	atf_check -o inline:'660 0 0\n' stat -f '%Lp %u %g' restricted
}
unix_socket_access_cleanup()
{
	stop_prowld
}

atf_test_case unit_explicit_settings cleanup
unit_explicit_settings_head()
{
	atf_set descr "Reject malformed chroot, umask, and socket addresses"
	atf_set require.user root
}
unit_explicit_settings_body()
{
	mkdir units
	while IFS='|' read -r name fields; do
		cat >"units/$name.unit" <<EOF
{"label":"org.test.settings_$name","program":"/usr/bin/true",$fields}
EOF
		printf '%s\n' "$name" >>rejected
	done <<'EOF'
root_empty|"root_directory":""
root_type|"root_directory":false
root_nul|"root_directory":"/\u0000ignored"
root_duplicate|"root_directory":"/","root_directory":"/var"
mask_empty|"umask":""
mask_type|"umask":22
mask_prefix|"umask":"0022junk"
mask_space|"umask":" 0022"
mask_signed|"umask":"-1"
mask_range|"umask":"1000"
mask_nul|"umask":"0022\u0000"
mask_duplicate|"umask":"0077","umask":"0000"
host_empty|"sockets":{"main":{"port":0,"host":""}}
host_type|"sockets":{"main":{"port":0,"host":false}}
host_invalid|"sockets":{"main":{"port":0,"host":"localhost"}}
host_nul|"sockets":{"main":{"port":0,"host":"127.0.0.1\u0000ignored"}}
host_duplicate|"sockets":{"main":{"port":0,"host":"127.0.0.1","host":"0.0.0.0"}}
host_no_port|"sockets":{"main":{"host":"127.0.0.1"}}
port_type|"sockets":{"main":{"port":"0"}}
port_fraction|"sockets":{"main":{"port":0.5}}
port_negative|"sockets":{"main":{"port":-1}}
port_range|"sockets":{"main":{"port":65536}}
port_wide|"sockets":{"main":{"port":4294967296}}
port_duplicate|"sockets":{"main":{"port":0,"port":1}}
path_empty|"sockets":{"main":{"path":""}}
path_type|"sockets":{"main":{"path":false,"port":0}}
path_nul|"sockets":{"main":{"path":"endpoint\u0000ignored"}}
path_duplicate|"sockets":{"main":{"path":"endpoint","path":"other"}}
path_conflict|"sockets":{"main":{"path":"endpoint","port":0}}
socket_duplicate|"sockets":{"main":{"port":0},"main":{"port":-1}}
sockets_duplicate|"sockets":{"main":{"port":0}},"sockets":{"main":{"port":-1}}
EOF
	long_root=$(printf '%01024d' 0)
	long_socket=$(printf '%0104d' 0)
	cat >units/root_long.unit <<EOF
{"label":"org.test.settings_root_long","program":"/usr/bin/true",
 "root_directory":"$long_root"}
EOF
	cat >units/path_long.unit <<EOF
{"label":"org.test.settings_path_long","program":"/usr/bin/true",
 "sockets":{"main":{"path":"$long_socket"}}}
EOF
	printf '%s\n' root_long path_long >>rejected

	for name in dropin_bad dropin_valid; do
		cat >"units/$name.unit" <<EOF
label = "org.test.settings_$name";
program = "/usr/bin/true";
EOF
		mkdir "units/org.test.settings_$name.d"
	done
	echo 'sockets { main { port = 0; host = false; } }' \
	    >units/org.test.settings_dropin_bad.d/socket.unit
	echo 'root_directory = "/"; user = "nobody"; umask = "0000";' \
	    >units/org.test.settings_dropin_valid.d/security.unit
	echo dropin_bad >>rejected

	cat >units/ipv6_valid.unit <<'EOF'
{"label":"org.test.settings_ipv6_valid","program":"/usr/bin/true",
 "sockets":{"loopback":{"port":0,"host":"::1"}}}
EOF
	cat >units/socket_valid.unit <<'EOF'
{"label":"org.test.settings_socket_valid","program":"/usr/bin/true",
 "run_at_load":true,"sockets":{"unix":{"path":"settings.sock"},
 "loopback":{"port":0,"host":"127.0.0.1"},"wildcard":{"port":0}}}
EOF
	start_prowld
	while read -r name; do
		atf_check -s exit:1 -o ignore -e match:'job not found' \
		    ${CTL} status "org.test.settings_$name"
	done <rejected
	atf_check -o ignore ${CTL} status org.test.settings_dropin_valid
	atf_check -o ignore ${CTL} status org.test.settings_ipv6_valid
	wait_state org.test.settings_socket_valid running
	[ -S settings.sock ] || atf_fail "missing valid activation socket"
}
unit_explicit_settings_cleanup()
{
	stop_prowld
}

atf_init_test_cases()
{
	atf_add_test_case action_deadline
	atf_add_test_case mask_activation
	atf_add_test_case persist_enabled
	atf_add_test_case masked_timer
	atf_add_test_case unix_socket_access
	atf_add_test_case interactive
	atf_add_test_case unit_explicit_settings
	atf_add_test_case credentials
	atf_add_test_case dependency_loop
	atf_add_test_case oneshot_lists
	atf_add_test_case post_hook_timeout
	atf_add_test_case standard_streams
	atf_add_test_case conditions
	atf_add_test_case activates
	atf_add_test_case pre_post
	atf_add_test_case forking
	atf_add_test_case forking_launch_identity
	atf_add_test_case forking_pidfile
	atf_add_test_case shim_pidfile_status
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
