#
# Regression tests for progress(1) security/robustness.
#

atf_test_case zflag_no_shell_injection
zflag_no_shell_injection_body()
{
	PROGRESS_BIN="$(ls /usr/obj/usr/src/*/usr.bin/progress/progress 2>/dev/null | head -n 1)"
	if [ -z "${PROGRESS_BIN}" ] || [ ! -x "${PROGRESS_BIN}" ]; then
		atf_skip "progress binary not found"
	fi
	atf_require_prog gzip

	# Create a gzipped file whose name contains shell metacharacters.
	echo "hello" > "in;echoINJECTED"
	gzip -c -- "in;echoINJECTED" > "in;echoINJECTED.gz"

	# If progress used popen("gzip -l %s"), this would run "echoINJECTED".
	atf_check -s exit:0 -o not-empty -e not-match:"INJECTED" \
	    "${PROGRESS_BIN}" -z -f "in;echoINJECTED.gz" cat >/dev/null
}

atf_init_test_cases()
{
	atf_add_test_case zflag_no_shell_injection
}
