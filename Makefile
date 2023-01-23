OBJS	= shell.o
SOURCE	= shell.c
OUT	    = result
CC	    = gcc
FLAGS	= -g3 -c -Wall
LFLAGS	= -lpthread

all: $(OBJS)
	$(CC) -g $(OBJS) -o $(OUT) $(LFLAGS)

result: $(OBJS)
	$(CC) -g $(OBJS) -o $(OUT)
shell1.o: shell.c
	$(CC) $(FLAGS) shell1.c -lcunit

clean:
	rm -f $(OBJS) $(OUT)

run: $(OUT)
	./$(OUT)

debug: $(OUT)
	valgrind $(OUT)

valgrind: $(OUT)
	valgrind $(OUT)

valgrind_leakcheck: $(OUT)
	valgrind --leak-check=full $(OUT)

valgrind_extreme: $(OUT)
	valgrind --leak-check=full --show-leak-kinds=all --leak-resolution=high --track-origins=yes --vgdb=yes $(OUT)