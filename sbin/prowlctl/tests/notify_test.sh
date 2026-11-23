# SPDX-License-Identifier: BSD-2-Clause

atf_test_case credentials cleanup
credentials_head()
{
	atf_set descr 'Notification credentials and non-root readiness'
	atf_set require.user root
}
credentials_body()
{
	PROWLD=$(atf_config_get prowld /sbin/prowld)
	PROWLCTL=$(atf_config_get prowlctl /sbin/prowlctl)
	[ -x "$PROWLD" ] || atf_skip 'prowld unavailable'
	[ -x "$PROWLCTL" ] || atf_skip 'prowlctl unavailable'
	id nobody >/dev/null 2>&1 || atf_skip 'nobody user unavailable'

	base=$(mktemp -d /tmp/prowl-notify.XXXXXX) || atf_fail 'mktemp failed'
	printf '%s\n' "$base" > fixture.path
	chmod 755 "$base"
	mkdir "$base/units" "$base/run" "$base/run/notify"
	chmod 755 "$base/units" "$base/run"
	chmod 700 "$base/run/notify"
	helper="$(atf_get_srcdir)/notify_helper"
	cat > "$base/units/test.unit" <<END
{"label":"test.notify", "type":"daemon", "program":"$helper",
 "user":"nobody", "notify_type":"notify", "run_at_load":true}
END
	cat > "$base/units/template@.unit" <<END
{"label":"test.template@%i", "type":"daemon", "program":"/bin/sleep",
 "arguments":["30"], "run_at_load":false}
END
	env -i PATH=/sbin:/bin:/usr/sbin:/usr/bin "$PROWLD" -d \
	    -s "$base/run" -u "$base/units" > "$base/log" 2>&1 &
	echo $! > fixture.pid
	for i in $(jot 100); do
		[ -S "$base/run/notify/test.notify" ] && break
		sleep 0.05
	done
	[ -S "$base/run/notify/test.notify" ] || atf_fail 'socket unavailable'
	atf_check -s exit:0 "$helper" "$base/run/notify/test.notify"
	sleep 0.2
	"$PROWLCTL" -s "$base/run" status test.notify > status ||
	    atf_fail 'status unavailable'
	grep -qi starting status || atf_fail 'wrong UID READY accepted'
	touch "$base/run/notify.gate"
	for i in $(jot 100); do
		"$PROWLCTL" -s "$base/run" status test.notify > status 2>/dev/null
		grep -qi running status && break
		sleep 0.1
	done
	grep -qi running status || atf_fail 'service READY not accepted'
	[ "$(stat -f %Lp "$base/run/notify")" = 711 ] ||
	    atf_fail 'notification directory mode incorrect'
	[ "$(stat -f %Lp "$base/run/notify/test.notify")" = 600 ] ||
	    atf_fail 'notification socket mode incorrect'
	[ "$(stat -f %u "$base/run/notify/test.notify")" = "$(id -u nobody)" ] ||
	    atf_fail 'notification socket owner incorrect'
	# Wheel may look up existing instances but must not create new ones.
	atf_check -s not-exit:0 -o ignore -e ignore "$helper" query \
	    "$PROWLCTL" "$base/run" template@one
	atf_check -s exit:0 -o ignore "$PROWLCTL" -s "$base/run" \
	    status template@one
	atf_check -s exit:0 -o ignore "$helper" query "$PROWLCTL" \
	    "$base/run" template@one
}
credentials_cleanup()
{
	if [ -f fixture.pid ]; then
		pid=$(cat fixture.pid)
		kill -TERM "$pid" 2>/dev/null || :
		for i in $(jot 50); do
			kill -0 "$pid" 2>/dev/null || break
			sleep 0.1
		done
		if kill -0 "$pid" 2>/dev/null; then
			echo "private prowld did not exit" >&2
			return 1
		fi
	fi
	if [ -f fixture.path ]; then
		base=$(cat fixture.path)
		case "$base" in /tmp/prowl-notify.*) rm -rf "$base" ;; esac
	fi
}
atf_init_test_cases()
{
	atf_add_test_case credentials
}
