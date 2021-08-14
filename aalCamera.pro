include(coverage.pri)

TEMPLATE = subdirs

SUBDIRS += \
    src \
    unittests

RESOURCES += qtubuntu-camera.qrc

OTHER_FILES += .qmake.conf
