# Makefile

sql_lexer.c:		sql_lexer.lex sql_lexer_utils.h
		flex --noline sql_lexer.lex
		cp lex.yy.c sql_lexer.c

clean:
	rm -f lex.yy.c sql_lexer.c
