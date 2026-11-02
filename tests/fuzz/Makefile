# libFuzzer harnesses.  "make" builds them with clang; "make run" gives
# each a short run, as CI does.

CC=		clang
CFLAGS=		-g -O1 -fsanitize=fuzzer,address,undefined
CPPFLAGS=	-D_NETBSD_SOURCE -I../../common -I../../akture \
		-I../../akture-conv
FUZZTIME?=	60

PROGS=		fuzz_akbg fuzz_akmsg fuzz_conf fuzz_exif

all: ${PROGS}

fuzz_akbg: fuzz_akbg.c ../../common/akbg.c
	${CC} ${CFLAGS} ${CPPFLAGS} -o $@ fuzz_akbg.c ../../common/akbg.c

fuzz_akmsg: fuzz_akmsg.c ../../common/akmsg.c ../../common/akutil.c
	${CC} ${CFLAGS} ${CPPFLAGS} -o $@ fuzz_akmsg.c \
	    ../../common/akmsg.c ../../common/akutil.c

fuzz_conf: fuzz_conf.c ../../akture/conf.c ../../common/akopt.c \
    ../../common/akparse.c
	${CC} ${CFLAGS} ${CPPFLAGS} -o $@ fuzz_conf.c ../../akture/conf.c \
	    ../../common/akopt.c ../../common/akparse.c

fuzz_exif: fuzz_exif.c ../../akture-conv/exif.c
	${CC} ${CFLAGS} ${CPPFLAGS} -o $@ fuzz_exif.c \
	    ../../akture-conv/exif.c

run: all
	for p in ${PROGS}; do ./$$p -max_total_time=${FUZZTIME} || exit 1; done

clean:
	rm -f ${PROGS} crash-* leak-* timeout-*
