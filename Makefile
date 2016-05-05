all: tcpconn

uninstall: remove

tcpconn: tcpconn.c
	$(CC) -Wall -g tcpconn.c -o tcpconn $(CFLAGS)

install:
	install -m 4755 ./tcpconn /usr/bin/tcpconn

remove:
	rm -f /usr/bin/tcpconn

clean:
	rm -f ./tcpconn
