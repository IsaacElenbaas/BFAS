CC=g++
INC_PATH=-I. -Iutil
LIB_PATH=#-L/home/dev_tools/apr/lib
LIBS=-lm
CFLAGS=-g -Wall -Wextra -O3
UTILS=$(patsubst %.cpp, %.o, $(wildcard ./util/*.cpp))

.PHONY: all
all: PFAS

PFAS: UI/main.h UI/Makefile UI/main.cpp shape.vert shape.frag $(wildcard ./*.glsl) util/shapes.h UI/PFAS.o $(UTILS)
	$(MAKE) -C UI
	mv UI/UI PFAS
UI/PFAS.o: PFAS.cpp $(wildcard ./*.h) $(wildcard ./util/*.h) UI/UI.cpp
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $< $(LIB_PATH) -I/usr/include/qt{,/QtGui,/QtWidgets} $(LIBS)
util/resource.o: util/resource.cpp util/resource.h types.h util/shapes.h
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $< $(LIB_PATH) $(LIBS)
util/shapes.o: util/shapes.cpp util/shapes.h types.h
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $< $(LIB_PATH) -I/usr/include/qt{,/QtGui,/QtWidgets} $(LIBS)

%.o: %.cpp %.h types.h
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $< $(LIB_PATH) $(LIBS)

# TODO: make components require UI/UI.h

UI/Makefile: UI/UI.pro $(UTILS)
	cd UI && qmake

.PHONY: clean
clean:
	rm -f PFAS
	rm -f *.o UI/*.o util/*.o
	rm -f UI/{Makefile,moc_*,.qmake.stash}
