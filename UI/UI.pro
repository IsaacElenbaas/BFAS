QT=core widgets opengl
LIBS=PFAS.o $$files(../util/*.o, true)
SOURCES=main.cpp color_picker.cpp
HEADERS=main.h ../shape.vert ../shape.frag color_picker.h color_picker.vert color_picker.frag ../util/shapes.h ../types.h
INCPATH=.. ../util
TARGET=UI
TEMPLATE=app
