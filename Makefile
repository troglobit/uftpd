
EXEC   = server
OBJS   = main.o ftpserver.o _string.o
LDLIBS = -lpthread

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) -o $(EXEC) $(OBJS) $(LDLIBS)

clean:
	-$(RM) $(EXEC) *.o

