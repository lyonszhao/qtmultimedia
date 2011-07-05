TEMPLATE = app
TARGET = audiorecorder

QT += multimediakit

HEADERS = \
    audiorecorder.h
  
SOURCES = \
    main.cpp \
    audiorecorder.cpp

maemo*: {
    FORMS += audiorecorder_small.ui
}else:symbian:contains(S60_VERSION, 3.2)|contains(S60_VERSION, 3.1){
    DEFINES += SYMBIAN_S60_3X
    FORMS += audiorecorder_small.ui
}else {
    FORMS += audiorecorder.ui
}
symbian: {
    TARGET.CAPABILITY = UserEnvironment ReadDeviceData WriteDeviceData
}

target.path = $$[QT_INSTALL_EXAMPLES]/qtmultimediakit/audiorecorder
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS *.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/qtmultimediakit/audiorecorder

INSTALLS += target sources
