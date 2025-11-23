CC = gcc

CFLAGS = -Wall -Wextra -O3 -I.

DEPS = defs.h board.h logic.h ai.h bot_easy.h bot_medium.h bot_hard.h network.h

OBJ = main.o board.o logic.o ai.o bot_easy.o bot_medium.o bot_hard.o network.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

connect4: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f *.o connect4 connect4.exe

.PHONY: clean
