TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += libs/qmldebug \
  plugins/qmlprofiler \
  plugins/qmlprofilerextended

QMAKE_EXTRA_TARGETS = docs install_docs # dummy targets for consistency
