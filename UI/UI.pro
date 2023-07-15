QT=core widgets opengl
LIBS=PFAS.o $$files(../util/*.o, true)
SOURCES=main.cpp
HEADERS=main.h ../shape.vert ../shape.frag ../util/shapes.h ../types.h
INCPATH=.. ../util
TARGET=UI
TEMPLATE=app
