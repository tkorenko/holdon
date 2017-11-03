CFLAGS	+= -g -Wall -pedantic
CFLAGS	+= -I/usr/local/include
LDFLAGS	+= -L/usr/local/lib
ifdef FREEBSD
LDLIBS	+= -linotify
endif
SRCDIR	:= src
BUILDDIR:= build
SRCS	:= $(wildcard ${SRCDIR}/*.c)
OBJS	:= $(addsuffix .o,$(basename $(notdir ${SRCS})))
OBJS	:= $(addprefix ${BUILDDIR}/,${OBJS})
PROG	:= holdon

$(shell mkdir -p ${BUILDDIR} > /dev/null)

${BUILDDIR}/%.o	: ${SRCDIR}/%.c
	${CC} ${CFLAGS} -c -o $@ $<

all	: ${PROG}

.PHONY	: view
view	:
	@echo "SRCS: ${SRCS}"
	@echo "OBJS: ${OBJS}"
	@echo "PROG: ${PROG}"

${PROG}	: ${OBJS}
	${CC} ${LDFLAGS} -o $@ $^ ${LDLIBS}

.PHONY	: clean
clean	:
	rm -rf *.core ${OBJS} ${PROG}
