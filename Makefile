CC=g++
INC_PATH=-I. -Iutil -IUI
LIB_PATH=#-L/home/dev_tools/apr/lib
LIBS=-lm
# for <bit> things
CFLAGS=-g -Wall -Wextra -O3 -std=c++20
UTILS=$(patsubst %.cpp, %.o, $(wildcard ./util/*.cpp))

.PHONY: all
all: PFAS

PFAS: UI/Makefile UI/main.cpp UI/main.h shape.vert shape.frag $(wildcard ./*.glsl) \
      UI/color_picker.cpp UI/color_picker.h UI/color_picker.vert UI/color_picker.frag \
      UI/settings_UI.cpp UI/settings_UI.h settings.h \
      util/shapes.h UI/PFAS.o $(UTILS)
	$(MAKE) -C UI
	mv UI/UI PFAS
UI/PFAS.o: PFAS.cpp $(wildcard ./*.h) $(wildcard ./util/*.h) UI/UI.cpp settings.h
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $< $(LIB_PATH) -I/usr/include/qt{,/QtGui,/QtWidgets} $(LIBS)
util/resource.o: util/resource.cpp util/resource.h types.h util/shapes.h
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $< $(LIB_PATH) $(LIBS)
util/shapes.o: util/shapes.cpp util/shapes.h types.h UI/UI.h util/quadtree.h util/resource.h
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $< $(LIB_PATH) -I/usr/include/qt{,/QtGui,/QtWidgets} $(LIBS)
util/util.o: util/util.cpp util/util.h types.h PFAS.h
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $< $(LIB_PATH) $(LIBS)

%.o: %.cpp %.h types.h
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $< $(LIB_PATH) $(LIBS)

# TODO: make components require UI/UI.h

UI/Makefile: UI/UI.pro
	cd UI && qmake

.PHONY: clean
clean:
	rm -f PFAS
	rm -f *.o UI/*.o util/*.o
	rm -f UI/{Makefile,moc_*,.qmake.stash}
