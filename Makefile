CC=gcc
CFLAGS=-g -Werror -Wall
# CPPFLAGS=<path_to_your_include>
# LDFLAGS=<path_to_your_lib>

all: ex1 ex2 ex3 ex4 ex5 ex6

ex6: ex6.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o ex6 ex6.c -luv
ex5: ex5.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o ex5 ex5.c -luv
ex4: ex4.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o ex4 ex4.c -luv
ex3: ex3.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o ex3 ex3.c -luv
ex2: ex2.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o ex2 ex2.c -luv
ex1: ex1.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o ex1 ex1.c -luv
