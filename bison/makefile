# Makefile

sql_lexer.c:		sql_lexer.lex 
		flex --noline sql_lexer.lex
		cp lex.yy.c sql_lexer.c

clean:
	rm -f lex.yy.c sql_lexer.c
