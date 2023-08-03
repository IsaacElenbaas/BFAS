QT=core widgets opengl
CONFIG+=c++20
LIBS=PFAS.o $$files(../util/*.o, true)
SOURCES=main.cpp color_picker.cpp settings_UI.cpp
HEADERS=main.h ../shape.vert ../shape.frag color_picker.h color_picker.vert color_picker.frag settings_UI.h ../settings.h ../util/shapes.h ../types.h
INCPATH=.. ../util
TARGET=UI
TEMPLATE=app
