# Makefile

OBJS	= bison.o lex.o main.o

CC	= g++
CFLAGS	= -g -Wall -ansi -pedantic

calc:		$(OBJS)
		$(CC) $(CFLAGS) $(OBJS) -o calc -ll

lex.o:		lex.c
		$(CC) $(CFLAGS) -c lex.c -o lex.o

lex.c:		sql_bdb.lex 
		flex sql_bdb.lex
		cp lex.yy.c lex.c

bison.o:	bison.c
		$(CC) $(CFLAGS) -c bison.c -o bison.o

bison.c:	sql_bdb.y
		bison -d -v sql_bdb.y
		cp sql_bdb.tab.c bison.c
		cmp -s sql_bdb.tab.h tok.h || cp sql_bdb.tab.h tok.h

main.o:		main.cc
		$(CC) $(CFLAGS) -c main.cc -o main.o

lex.o yac.o main.o	: heading.h
lex.o main.o		: tok.h

clean:
	rm -f *.o *~ lex.c lex.yy.c bison.c tok.h sql_bdb.tab.c sql_bdb.tab.h sql_bdb.output calc
