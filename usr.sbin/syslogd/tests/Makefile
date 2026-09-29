PACKAGE=	tests

ATF_TESTS_SH=	syslogd_basic_format_test \
		syslogd_forwarded_format_test \
		syslogd_relayed_format_test \
		syslogd_test

# Kyua 0.13 lacks the "jail" execenv used upstream, so run these serially:
# several syslogd instances listening on the same port cannot run in parallel.
TEST_METADATA=	is_exclusive="true" \
		timeout=20

${PACKAGE}FILES=syslogd_format_test_common.sh \
		syslogd_test_common.sh

.include <bsd.test.mk>
