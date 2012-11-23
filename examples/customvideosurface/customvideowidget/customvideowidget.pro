TEMPLATE = app
TARGET = customvideowidget

QT += multimedia multimediawidgets widgets

HEADERS = \
    videoplayer.h \
    videowidget.h \
    videowidgetsurface.h

SOURCES = \
    main.cpp \
    videoplayer.cpp \
    videowidget.cpp \
    videowidgetsurface.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/qtmultimedia/customvideosurface/customvideowidget
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS *.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/qtmultimedia/customvideosurface/customvideowidget

INSTALLS += target sources