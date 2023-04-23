CC=g++
INC_PATH=-I. -Iutil
LIB_PATH=#-L/home/dev_tools/apr/lib
LIBS=-lm
CFLAGS=-Wall -Wextra -O3 $(INC_PATH)
UTILS=$(patsubst %.cpp, %.o, $(wildcard ./util/*.cpp))

.PHONY: all
all: PFAS

PFAS: UI/PFAS.o $(UTILS) UI/main.cpp UI/main.h UI/Makefile
	$(MAKE) -C UI
	mv UI/UI PFAS
UI/PFAS.o: PFAS.cpp $(wildcard ./*.h) $(wildcard ./util/*.h) UI/UI.cpp
	$(CC) $(CFLAGS) -c -o $@ $< $(LIB_PATH) -I/usr/include/qt{,/QtGui,/QtWidgets} $(LIBS)

%.o: %.cpp %.h ./types.h
	$(CC) $(CFLAGS) -c -o $@ $< $(LIB_PATH) $(LIBS)

# TODO: make components require UI/UI.h

UI/Makefile: UI/UI.pro $(UTILS)
	cd UI && qmake

.PHONY: clean
clean:
	rm -f PFAS
	rm -f *.o UI/*.o util/*.o
	rm -f UI/{Makefile,moc_*,.qmake.stash}
