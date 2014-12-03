QT += qml quick

HEADERS += mpvrenderer.h
SOURCES += mpvrenderer.cpp main.cpp

LIBS += -L../../../build/ -lmpv
INCLUDEPATH = ../../../
DEPENDPATH = ../../../

RESOURCES += mpvtest.qrc

OTHER_FILES += main.qml
