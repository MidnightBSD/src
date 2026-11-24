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

# Exercise normal unit discovery and rc.conf lookups inside a private root.
policy_fixture()
{
	prowld=$(atf_config_get prowld /sbin/prowld)
	ctl=$(atf_config_get prowlctl /sbin/prowlctl)
	umask 022
	base=root/etc/prowld/units.d
	localunits=root/usr/local/etc/prowld/units.d
	admin=root/etc/prowld/overrides.d
	mkdir -p "$base" "$localunits" "$admin" root/etc/rc.d root/bin \
	    root/dev root/run root/results
	: >root/dev/null
	cp "$prowld" root/prowld
	cp /rescue/sh root/bin/sh
	cat >root/base <<'END'
#!/bin/sh
printf '%s\n' "${0##*/}" >"/results/$1"
END
	chmod 755 root/base
	for name in local admin forbidden; do
		ln root/base "root/$name"
	done
}

policy_unit()
{
	cat >"$1" <<END
label = "test.$2";
rc_name = "$4";
type = "oneshot";
program = "$3";
arguments = ["$4"];
run_at_load = true;
END
}

policy_start()
{
	env -i PATH=/bin:/usr/bin:/sbin:/usr/sbin chroot root /prowld \
	    -d -s /run >fixture.log 2>&1 &
	echo $! >fixture.pid
	for i in $(jot 100); do
		"$ctl" -s root/run list >/dev/null 2>&1 && return 0
		sleep 0.1
	done
	cat fixture.log
	atf_fail 'prowld failed to start'
}

policy_result()
{
	for i in $(jot 100); do
		[ -s "root/results/$1" ] && break
		sleep 0.1
	done
	if [ ! -s "root/results/$1" ]; then
		cat fixture.log
		atf_fail "missing service result for $1"
	fi
	atf_check_equal "$2" "$(cat "root/results/$1")"
}

atf_test_case unit_policy cleanup
unit_policy_head()
{
	atf_set descr 'Administrator policy takes precedence and rejected native services cannot start through rc.d'
	atf_set require.user root
	atf_set require.progs 'chroot /rescue/sh'
}
unit_policy_body()
{
	policy_fixture
	# Labels establish precedence even when filenames differ.
	policy_unit "$base/original.unit" winner /base winner
	policy_unit "$localunits/package.unit" winner /local winner
	policy_unit "$admin/replacement.unit" winner /admin winner
	policy_unit "$base/partial.unit" partial /base partial
	mkdir "$base/test.partial.d" "$admin/test.partial.d"
	echo 'program = "/local";' >"$base/test.partial.d/package.unit"
	# Deliberately create the later file first to exercise lexical ordering.
	echo 'program = "/admin";' >"$admin/test.partial.d/90-final.unit"
	echo 'program = "/base";' >"$admin/test.partial.d/10-first.unit"

	policy_unit "$base/disabled.unit" blocked /forbidden blocked
	echo 'provides = ["servicealias"];' >>"$base/disabled.unit"
	mkdir "$admin/test.blocked.d"
	cat >"$admin/test.blocked.d/security.unit" <<'END'
provides = ["overriddenalias"];
umask = "invalid";
END
	cat >root/etc/rc.d/legacy <<'END'
#!/bin/sh
# PROVIDE: servicealias
echo legacy >/results/legacy
END
	chmod 755 root/etc/rc.d/legacy
	cat >root/etc/rc.d/overridelegacy <<'END'
#!/bin/sh
# PROVIDE: overriddenalias
echo overridelegacy >/results/overridelegacy
END
	chmod 755 root/etc/rc.d/overridelegacy
	policy_unit "$base/shadow.unit" shadow /forbidden shadow
	echo 'provides = ["shadowalias"];' >>"$base/shadow.unit"
	cp "$base/shadow.unit" "$admin/shadow.unit"
	echo 'umask = "invalid";' >>"$admin/shadow.unit"
	cat >root/etc/rc.d/shadowrc <<'END'
#!/bin/sh
# PROVIDE: shadowalias
echo shadowrc >/results/shadowrc
END
	chmod 755 root/etc/rc.d/shadowrc
	# A native unit and a legacy script can share only their public label.
	cat >"$admin/label-only.unit" <<'END'
label = "org.midnightbsd.rc.labelonly";
rc_name = "nativeprovider";
program = "/forbidden";
umask = "invalid";
END
	cat >root/etc/rc.d/labelonly <<'END'
#!/bin/sh
# PROVIDE: legacyprovider
echo legacy >/results/labelonly
END
	chmod 755 root/etc/rc.d/labelonly
	cat >root/etc/rc.d/control <<'END'
#!/bin/sh
# PROVIDE: control
echo control >/results/control
END
	chmod 755 root/etc/rc.d/control
	cat >root/etc/rc.d/latelegacy <<'END'
#!/bin/sh
# PROVIDE: lateprovider
rcvar=late_enable
echo legacy >/results/latelegacy
END
	chmod 755 root/etc/rc.d/latelegacy
	cat >"$base/idle.unit" <<'END'
label = "test.idle";
type = "oneshot";
program = "/forbidden";
arguments = ["idle"];
check_arguments = ["idlecheck"];
run_at_load = false;
END
	policy_unit "$base/unrelated-name.unit" oldidentity /base oldidentity

	mkfifo root/hold
	cat >root/active <<'END'
#!/bin/sh
if [ "$1" = check ]; then
	echo active-ok
	exit 0
fi
echo "$$" >/results/active.pid
read line </hold
END
	chmod 755 root/active
	cat >"$base/active.unit" <<'END'
label = "test.active";
type = "daemon";
program = "/active";
check_arguments = ["check"];
run_at_load = true;
END
	policy_unit "$base/example@.unit" 'example@%i' /base '%i'
	echo 'default_instance = "lower";' >>"$base/example@.unit"
	policy_unit "$admin/replacement@.unit" 'example@%i' /admin '%i'
	echo 'default_instance = "boot";' >>"$admin/replacement@.unit"

	policy_start
	policy_result winner admin
	policy_result partial admin
	policy_result boot admin
	atf_check -o ignore "$ctl" -s root/run start 'test.example@lazy'
	policy_result lazy admin
	atf_check -o ignore "$ctl" -s root/run start 'example@short'
	policy_result short admin
	atf_check -o ignore "$ctl" -s root/run start org.midnightbsd.rc.control
	policy_result control control
	for target in test.blocked org.midnightbsd.rc.legacy \
	    org.midnightbsd.rc.overridelegacy test.shadow org.midnightbsd.rc.shadowrc \
	    org.midnightbsd.rc.labelonly; do
		atf_check -s not-exit:0 -o ignore -e ignore \
		    "$ctl" -s root/run start "$target"
	done
	for name in lower blocked legacy overridelegacy shadow shadowrc labelonly; do
		[ ! -e "root/results/$name" ] || atf_fail "unexpected launch of $name"
	done

	active=$(cat root/results/active.pid) || atf_fail 'active daemon did not start'
	atf_check -o match:"PID: ${active}$" "$ctl" -s root/run status test.active
	atf_check -o ignore "$ctl" -s root/run status org.midnightbsd.rc.latelegacy
	atf_check -o ignore "$ctl" -s root/run status test.idle
	mkdir "$admin/test.idle.d"
	echo 'umask = "invalid";' >"$admin/test.idle.d/security.unit"
	mkdir "$admin/test.example@lazy.d"
	echo 'umask = "invalid";' >"$admin/test.example@lazy.d/security.unit"
	echo 'label = {' >"$base/unrelated-name.unit"
	policy_unit "$admin/new.unit" new /forbidden lateprovider
	echo 'umask = "invalid";' >>"$admin/new.unit"
	echo 'program = "/admin";' >"$admin/test.blocked.d/security.unit"
	atf_check -o ignore "$ctl" -s root/run reload-config
	policy_result blocked admin
	atf_check -o match:"PID: ${active}$" "$ctl" -s root/run status test.active
	atf_check -o ignore kill -0 "$active"
	atf_check -o match:active-ok "$ctl" -s root/run check test.active
	[ ! -e root/results/legacy ] || atf_fail 'legacy service ran on reload'
	for target in test.idle test.oldidentity 'test.example@lazy' \
	    org.midnightbsd.rc.latelegacy; do
		atf_check -s not-exit:0 -o ignore -e ignore \
		    "$ctl" -s root/run start "$target"
	done
	atf_check -s not-exit:0 -o ignore -e ignore \
	    "$ctl" -s root/run check test.idle
	for name in idle idlecheck lateprovider latelegacy; do
		[ ! -e "root/results/$name" ] || atf_fail "stale policy launched $name"
	done

	# An explicit restart after a valid policy change uses the replacement.
	cat >root/newactive <<'END'
#!/bin/sh
echo "$$" >/results/newactive.pid
read line </hold
END
	chmod 755 root/newactive
	mkdir "$admin/test.active.d"
	printf '%s\n' 'program="/newactive";' 'run_at_load=false;' \
	    >"$admin/test.active.d/changed.unit"
	policy_unit "$base/rescanned.unit" rescanned /local rescanned
	atf_check -o ignore "$ctl" -s root/run reload-config
	policy_result rescanned local
	atf_check -o ignore "$ctl" -s root/run restart test.active
	for i in $(jot 100); do
		[ -s root/results/newactive.pid ] && break
		sleep 0.1
	done
	replacement=$(cat root/results/newactive.pid) ||
	    atf_fail 'restart did not apply the replacement policy'
	[ "$replacement" != "$active" ] || atf_fail 'restart kept the old daemon'
	atf_check -o match:"PID: ${replacement}$" "$ctl" -s root/run status test.active
}
unit_policy_cleanup()
{
	trusted_inputs_cleanup
}

atf_test_case rcconf_policy cleanup
rcconf_policy_head()
{
	atf_set descr 'Trusted rc.conf files and ordered fragments select native service executables'
	atf_set require.user root
	atf_set require.progs 'chroot /rescue/sh'
}
rcconf_policy_body()
{
	policy_fixture
	etcconf=root/etc/rc.conf.d
	localconf=root/usr/local/etc/rc.conf.d
	mkdir -p "$etcconf/ordered" "$etcconf/baddir" "$localconf/layered"
	for name in global flat ordered layered badfile baddir link; do
		policy_unit "$base/$name.unit" "$name" '@policy_program@' "$name"
	done

	echo 'policy_program="/base"' >root/etc/rc.conf
	cat >root/etc/rc.conf.local <<'END'
policy_program="/forbidden"
global_enable="NO"
END
	chmod g+w root/etc/rc.conf.local
	echo 'policy_program="/local"' >"$etcconf/flat"
	echo 'policy_program="/local"' >"$etcconf/ordered/90-last"
	echo 'policy_program="/base"' >"$etcconf/ordered/10-first"
	echo 'policy_program="/forbidden"' >"$etcconf/ordered/.hidden"
	echo 'policy_program="/local"' >"$etcconf/layered"
	echo 'policy_program="/admin"' >"$localconf/layered/20-program"
	echo 'policy_program="/forbidden"' >"$etcconf/badfile"
	chmod g+w "$etcconf/badfile"
	echo 'policy_program="/forbidden"' >"$etcconf/baddir/10-program"
	chmod g+w "$etcconf/baddir"
	echo 'policy_program="/forbidden"' >root/linked.conf
	ln -s /linked.conf "$etcconf/link"
	if command -v setfacl >/dev/null &&
	    getfacl "$etcconf" 2>/dev/null | grep -q 'owner@:'; then
		policy_unit "$base/acl.unit" acl '@policy_program@' acl
		echo 'policy_program="/forbidden"' >"$etcconf/acl"
		atf_check setfacl -a 0 'user:65534:C::allow' "$etcconf/acl"
		atf_check -o inline:'644\n' stat -f '%Lp' "$etcconf/acl"
	fi

	policy_start
	for name in global badfile baddir link; do
		policy_result "$name" base
	done
	policy_result flat local
	policy_result ordered local
	policy_result layered admin
	if [ -f "$base/acl.unit" ]; then
		policy_result acl base
	fi
}
rcconf_policy_cleanup()
{
	trusted_inputs_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case trusted_inputs
	atf_add_test_case nfsv4_acl
	atf_add_test_case unit_policy
	atf_add_test_case rcconf_policy
}
