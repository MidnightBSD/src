divert(-1)
#
# Copyright (c) 2025 Proofpoint, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)
VERSIONID(`$Id: same_domain_only.m4,v 1.1 2025-10-20 18:00:00 ca Exp $')
LOCAL_CONFIG
ifelse(len(X`'_ARG1_), `1', `dnl
O SameDomainOnly=Multiple destination domains per transaction is unsupported
O SameDomainOnly=Address domain different from previous accepted address.
', `dnl
ifelse(len(X`'_ARG1_), `1', `dnl', `O SameDomainOnly=_ARG1_')
ifelse(len(X`'_ARG2_), `1', `dnl', `O SameDomainOnly=_ARG2_')
ifelse(len(X`'_ARG3_), `1', `dnl', `O SameDomainOnly=_ARG3_')
ifelse(len(X`'_ARG4_), `1', `dnl', `O SameDomainOnly=_ARG4_')
ifelse(len(X`'_ARG5_), `1', `dnl', `O SameDomainOnly=_ARG5_')
ifelse(len(X`'_ARG6_), `1', `dnl', `O SameDomainOnly=_ARG6_')
ifelse(len(X`'_ARG7_), `1', `dnl', `O SameDomainOnly=_ARG7_')
ifelse(len(X`'_ARG8_), `1', `dnl', `O SameDomainOnly=_ARG8_')
ifelse(len(X`'_ARG9_), `1', `dnl', `O SameDomainOnly=_ARG9_')
')dnl
