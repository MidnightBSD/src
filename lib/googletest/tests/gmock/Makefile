
.include <bsd.init.mk>

.PATH: ${GOOGLEMOCK_SRCROOT}/src ${GOOGLEMOCK_SRCROOT}/test

GTESTS+=	gmock_stress_test

LIBADD+=	pthread gtest gmock

# The next release will resolve a number of build warnings issues.
MK_WERROR=	no

.include <bsd.test.mk>
