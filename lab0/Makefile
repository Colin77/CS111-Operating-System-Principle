CC = gcc

TARGET = get

$(TARGET): $(TARGET).o
	$(CC) -o $(TARGET) $(TARGET).o 

.PHONY: clean

clean:
	$(RM)  $(TARGET) *.o *~
dist:
	tar -czvf get.tar.gz get.c Makefile
