# Formal verification with Frama-C, in one session so that the results
# of WP and Eva are combined.  Versions are pinned in VERSIONS; CI runs
# "make" and rejects changes that do not prove.

FRAMAC?=	frama-c
PROVERS?=	alt-ergo,z3,cvc5
TIMEOUT?=	30

SRCDIR=		${.CURDIR}/..
FCPP=		-cpp-extra-args="-I${.CURDIR}/stubs -I${SRCDIR}/common \
		-I${SRCDIR}/akture -include ${.CURDIR}/stubs/akture_spec.h"
SRCS=		${.CURDIR}/eva_main.c \
		${SRCDIR}/common/akbg.c ${SRCDIR}/common/akmsg.c \
		${SRCDIR}/common/akopt.c ${SRCDIR}/common/akparse.c \
		${SRCDIR}/common/akutil.c ${SRCDIR}/akture/conf.c \
		${SRCDIR}/akture/state.c

# WP with run-time error checks: what the master decides on.  Message
# parsing (no out of bounds access or overflow), the state machine (no
# session without authentication), the order of the privilege drop,
# the erasure of secrets and the .akbg header.  Volatile is ignored:
# it only keeps the compiler from dropping the stores of akmsg_clear().
WP_CORE=	akmsg_layout,akmsg_okbyte,akmsg_init,akmsg_encode,\
		akmsg_decode,akmsg_clear,ak_explicit_zero,ak_privdrop,\
		akbg_check,\
		akstate_valid,akstate_accepts,akstate_next,akstate_fail_delay

# WP for the contracts of the lexical helpers; Eva covers their
# run-time errors.
WP_LEX=		ak_copy,isspc,iskeychar,hexval,ak_parse_line,ak_split_words,\
		ak_parse_color,ak_parse_bool,ak_parse_scale,\
		isprintable,isname,islayouts

WP_FLAGS=	-wp-no-volatile -wp-prover ${PROVERS} -wp-timeout ${TIMEOUT}

# Eva from eva_main.c: the configuration reader from the file, argv
# splitting, the .akbg header and messages, all on arbitrary input.
EVA_FLAGS=	-eva-precision 3 -eva-split-return auto -main main

# Unknown or invalid properties fail the run.  Untried ones are listed
# for review: contracts of the library stubs, and preconditions that
# only callers outside the analysed code establish.
REPORT=		-report-classify -report-unclassified-unknown ERROR \
		-report-unclassified-invalid ERROR \
		-report-unclassified-error ERROR \
		-report-unclassified-untried REVIEW \
		-report-unclassified-warning REVIEW

all:
	${FRAMAC} ${FCPP} ${SRCS} \
	    -wp -wp-rte -wp-fct ${WP_CORE:ts} ${WP_FLAGS} \
	    -then -wp-no-rte -wp-fct ${WP_LEX:ts} \
	    -then -no-wp -eva ${EVA_FLAGS} \
	    -then ${REPORT}

.PHONY: all
