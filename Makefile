EXEC   = uftpd
OBJS   = uftpd.o ftpcmd.o string.o
LDLIBS = -lpthread

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) -o $(EXEC) $(OBJS) $(LDLIBS)

clean:
	-$(RM) $(EXEC) *.o

