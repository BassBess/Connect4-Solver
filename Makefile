CC = gcc
CFLAGS = -Wall -Wextra -I.

DEPS = defs.h board.h logic.h ai.h bot_easy.h bot_medium.h bot_hard.h

OBJ = main.o board.o logic.o ai.o bot_easy.o bot_medium.o bot_hard.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

connect4: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f *.o connect4
