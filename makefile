TARGET = server
SRC = server.c event.c http.c socket.c threadpool.c log.c
OBJ = $(patsubst %.c, %.o, $(SRC))
CC = cc

$(TARGET):$(OBJ)
	$(CC) -o $@ $(SRC)


.PYTHON:clean

clean:
	-rm -f $(OBJ)
