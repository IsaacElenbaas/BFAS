QT += core widgets
LIBS += PFAS.o $$files(../util/*.o, true)
SOURCES += main.cpp
HEADERS += main.h
TARGET = UI
TEMPLATE = app
