CC = gcc

CFLAGS  = -w -l pthread -l wiringPi
TARGET = fppd
all: $(TARGET)
$(TARGET): $(TARGET).c E131.c mpg123.c pixelnetDMX.c playList.c schedule.c command.c
  $(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c E131.c mpg123.c pixelnetDMX.c playList.c schedule.c command.c

clean:
  $(RM) $(TARGET)