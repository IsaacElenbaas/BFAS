CC=$(CROSS)g++
INC_PATH=-I. -Iutil -IUI
LIB_PATH=#-L/home/dev_tools/apr/lib
LIBS=-lm
# c++20 for <bit> things
CFLAGS=-Wall -Wextra -O3 -std=c++20
QT_INC_SUBFOLDERS=QtGui QtWidgets
QT_INC_PATH=$(addprefix -I, \
	$(foreach path, /mxe/usr/i686-w64-mingw32.static/qt5/include /usr/include/qt, \
		$(foreach dir, . $(addsuffix ., $(addprefix /, $(QT_INC_SUBFOLDERS))), \
		$(path)$(basename $(dir))) \
	) \
)
UTILS=$(patsubst %.cpp, %.o, $(wildcard ./util/*.cpp))

.PHONY: all
all: BFAS
.PHONY: release
release: BFAS BFAS.exe

BFAS.exe:
	docker run -i -v "$PWD:/src" openscad/mxe-requirements bash < build-win
BFAS: UI/Makefile UI/main.cpp UI/main.h shape.vert shape.frag $(wildcard ./*.glsl) \
      UI/color_picker.cpp UI/color_picker.h UI/color_picker.vert UI/color_picker.frag \
      UI/settings_UI.cpp UI/settings_UI.h settings.h \
      util/shapes.h UI/BFAS.o $(UTILS)
	$(MAKE) -C UI
ifneq (,$(wildcard ./UI/UI))
	mv UI/UI BFAS
else
	mv UI/release/UI.exe /src/BFAS.exe
	@touch BFAS
endif
UI/BFAS.o: BFAS.cpp $(wildcard ./*.h) $(wildcard ./util/*.h) UI/UI.cpp settings.h
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $< $(LIB_PATH) $(QT_INC_PATH) $(LIBS)
util/resource.o: util/resource.cpp util/resource.h types.h util/shapes.h
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $< $(LIB_PATH) $(LIBS)
util/shapes.o: util/shapes.cpp util/shapes.h types.h UI/UI.h util/quadtree.h util/resource.h
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $< $(LIB_PATH) $(QT_INC_PATH) $(LIBS)
util/util.o: util/util.cpp util/util.h types.h BFAS.h
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $< $(LIB_PATH) $(LIBS)

%.o: %.cpp %.h types.h
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $< $(LIB_PATH) $(LIBS)

# TODO: make components require UI/UI.h

UI/Makefile: UI/UI.pro
	cd UI && $(CROSS)qmake-qt5

.PHONY: clean
clean:
	rm -f BFAS
	rm -f *.o UI/*.o util/*.o
	rm -f UI/Makefile* UI/moc_* UI/.qmake.stash
	rm -rf UI/{debug,release}
