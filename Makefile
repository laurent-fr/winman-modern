all: winman

winman: winman.o isIcon.o
	gcc -g winman.c isIcon.c -o winman -L/usr/X11R6/lib -lX11 -lbsd

winman.o: winman.c winman.h
	gcc -c -g -Wall winman.c

isIcon.o: isIcon.c isIcon.h
	gcc -c -g -Wall isIcon.c

clean:
	rm -rf *.o winman
