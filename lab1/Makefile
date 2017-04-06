CC = gcc

TARGET = simpsh

$(TARGET): $(TARGET).o
	$(CC) -o $(TARGET) $(TARGET).o

.PHONY:clean

clean: 
	$(RM) $(TARGET) *.o *~

dist:
	tar -czvf $(TARGET).tar.gz $(TARGET).c Makefile
