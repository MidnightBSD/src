# SPDX-License-Identifier: BSD-2-Clause

atf_test_case trusted_inputs cleanup
trusted_inputs_head()
{
	atf_set descr 'Load trusted units, templates and drop-ins through a sticky ancestor'
	atf_set require.user root
}
trusted_inputs_body()
{
	prowld=$(atf_config_get prowld /sbin/prowld)
	ctl=$(atf_config_get prowlctl /sbin/prowlctl)
	[ -x "$prowld" ] || atf_skip 'prowld unavailable'
	[ -x "$ctl" ] || atf_skip 'prowlctl unavailable'
	[ -k /tmp ] || atf_skip '/tmp is not sticky'

	umask 022
	base=$(mktemp -d /tmp/prowl-trust.XXXXXX) || atf_fail 'mktemp failed'
	printf '%s\n' "$base" > fixture.path
	mkdir "$base/units" "$base/run" "$base/units/test.good.d"
	cat > "$base/units/good.unit" <<'END'
label = "test.good";
type = "oneshot";
program = "/usr/bin/true";
END
	cat > "$base/units/test.good.d/good.unit" <<'END'
description = "${CURDIR}/trusted";
END
	for label in badmode badidentity baddir; do
		mkdir "$base/units/test.$label.d"
		cat > "$base/units/$label.unit" <<END
label = "test.$label";
type = "oneshot";
program = "/usr/bin/touch";
arguments = ["$base/$label.started"];
run_at_load = true;
END
	done
	printf 'description = "unsafe-dropin";\n' \
	    > "$base/units/test.badmode.d/unsafe.unit"
	chmod g+w "$base/units/test.badmode.d/unsafe.unit" \
	    "$base/units/test.baddir.d"
	printf 'user = [];\n' > "$base/units/test.badidentity.d/user.unit"
	cat > "$base/units/template@.unit" <<'END'
label = "test.template@%i";
type = "oneshot";
program = "/usr/bin/true";
default_instance = "default";
END
	cat > "$base/units/unsafe@.unit" <<'END'
label = "test.unsafe@%i";
program = "/usr/bin/true";
default_instance = "default";
END
	cat > "$base/units/unsafe.unit" <<'END'
label = "test.unsafe";
program = "/usr/bin/true";
END
	chmod g+w "$base/units/unsafe.unit" "$base/units/unsafe@.unit"
	cat > "$base/units/target.conf" <<'END'
label = "test.link";
program = "/usr/bin/true";
END
	ln -s target.conf "$base/units/link.unit"
	mkfifo "$base/units/fifo.unit"

	env -i PATH=/sbin:/bin:/usr/sbin:/usr/bin "$prowld" -d \
	    -s "$base/run" -u "$base/units" > "$base/log" 2>&1 &
	echo $! > fixture.pid
	for i in $(jot 100); do
		if "$ctl" -s "$base/run" show 'test.template@default' \
		    > /dev/null 2>&1; then
			break
		fi
		sleep 0.05
	done
	atf_check -s exit:0 -o ignore "$ctl" -s "$base/run" show \
	    'test.template@default'
	atf_check -s exit:0 -o save:good.out "$ctl" -s "$base/run" show test.good
	atf_check -o ignore grep -F "$base/units/test.good.d/trusted" good.out
	for label in test.unsafe 'test.unsafe@default' test.link test.badmode \
	    test.badidentity test.baddir; do
		atf_check -s not-exit:0 -e ignore -o ignore \
		    "$ctl" -s "$base/run" show "$label"
	done
	for label in badmode badidentity baddir; do
		[ ! -e "$base/$label.started" ] ||
		    atf_fail "base unit started despite rejected drop-in: $label"
	done
}
trusted_inputs_cleanup()
{
	if [ -f fixture.pid ]; then
		pid=$(cat fixture.pid)
		kill -TERM "$pid" 2>/dev/null || :
	fi
	if [ -f fixture.path ]; then
		base=$(cat fixture.path)
		case "$base" in /tmp/prowl-trust.*) rm -rf "$base" ;; esac
	fi
}

atf_test_case nfsv4_acl cleanup
nfsv4_acl_head()
{
	atf_set descr 'Reject NFSv4 ACL mutation grants on trusted unit paths'
	atf_set require.user root
	atf_set require.progs 'getfacl setfacl stat'
}

start_acl_test_daemon()
{
	rundir=$1
	mkdir "$rundir"
	env -i PATH=/sbin:/bin:/usr/sbin:/usr/bin "$prowld" -d \
	    -s "$rundir" -u "$units" > "$rundir/log" 2>&1 &
	echo $! > fixture.pid
	for i in $(jot 100); do
		if "$ctl" -s "$rundir" list > /dev/null 2>&1; then
			return
		fi
		sleep 0.05
	done
	cat "$rundir/log"
	atf_fail 'prowld did not start'
}

nfsv4_acl_body()
{
	prowld=$(atf_config_get prowld /sbin/prowld)
	ctl=$(atf_config_get prowlctl /sbin/prowlctl)
	[ -x "$prowld" ] || atf_skip 'prowld unavailable'
	[ -x "$ctl" ] || atf_skip 'prowlctl unavailable'

	umask 022
	base=$(mktemp -d /tmp/prowl-trust.XXXXXX) || atf_fail 'mktemp failed'
	printf '%s\n' "$base" > fixture.path
	getfacl "$base" > acl.out || atf_fail 'getfacl failed'
	grep -q 'owner@:' acl.out || atf_skip '/tmp lacks NFSv4 ACLs'

	mkdir "$base/sticky" "$base/sticky/units"
	chmod 1777 "$base/sticky"
	units=$base/sticky/units
	for label in harmless ordered inherit final w p A W C o d; do
		cat > "$units/$label.unit" <<END
label = "test.$label";
type = "oneshot";
program = "/usr/bin/true";
END
	done
	atf_check setfacl -a 0 user:65534:r::allow "$units/harmless.unit"
	atf_check setfacl -a 0 user:65534:C::allow "$units/ordered.unit"
	atf_check setfacl -a 0 user:65534:C::deny "$units/ordered.unit"
	for right in w p A W C o d; do
		atf_check setfacl -a 0 "user:65534:$right::allow" \
		    "$units/$right.unit"
		atf_check -o inline:'644\n' stat -f '%Lp' "$units/$right.unit"
	done
	mkdir "$units/test.inherit.d" "$units/test.final.d"
	atf_check setfacl -a 0 user:65534:C:fi:allow "$units/test.inherit.d"
	atf_check setfacl -a 0 user:65534:D::allow "$units/test.final.d"

	start_acl_test_daemon "$base/run"
	for label in harmless ordered inherit; do
		atf_check -o ignore "$ctl" -s "$base/run" show "test.$label"
	done
	for label in w p A W C o d final; do
		atf_check -s not-exit:0 -o ignore -e ignore \
		    "$ctl" -s "$base/run" show "test.$label"
	done

	pid=$(cat fixture.pid)
	kill -TERM "$pid" || atf_fail 'could not stop prowld'
	wait "$pid" || atf_fail 'prowld failed during shutdown'
	rm fixture.pid
	atf_check setfacl -a 0 group:65534:D::allow "$base/sticky"
	[ -k "$base/sticky" ] || atf_fail 'sticky ancestor lost the sticky bit'
	atf_check -o match:'group:65534:[^:]*D[^:]*:[^:]*:allow' \
	    getfacl -n "$base/sticky"
	start_acl_test_daemon "$base/run-unsafe"
	atf_check -s not-exit:0 -o ignore -e ignore \
	    "$ctl" -s "$base/run-unsafe" show test.harmless
}
nfsv4_acl_cleanup()
{
	trusted_inputs_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case trusted_inputs
	atf_add_test_case nfsv4_acl
}
