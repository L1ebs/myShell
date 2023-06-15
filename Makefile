CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic

mysh: mysh.c
	$(CC) $(CFLAGS) mysh.c -o mysh

clean:
	rm mysh
