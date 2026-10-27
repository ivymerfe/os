TARGET = mimimi
SRCS = main.c uthread.c

CC=gcc
RM=rm
CFLAGS= -g -Wall
LIBS=
INCLUDE_DIR="."

all: ${TARGET}

${TARGET}: ${SRCS}
	${CC} ${CFLAGS} -I${INCLUDE_DIR} ${SRCS} ${LIBS} -o ${TARGET}

clean:
	${RM} -f *.o ${TARGET}
