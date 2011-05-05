QT.declarative.VERSION = 4.8.0
QT.declarative.MAJOR_VERSION = 4
QT.declarative.MINOR_VERSION = 8
QT.declarative.PATCH_VERSION = 0

QT.declarative.name = QtDeclarative
QT.declarative.bins = $$QT_MODULE_BIN_BASE
QT.declarative.includes = $$QT_MODULE_INCLUDE_BASE $$QT_MODULE_INCLUDE_BASE/QtDeclarative
QT.declarative.private_includes = $$QT_MODULE_INCLUDE_BASE/QtDeclarative/$$QT.declarative.VERSION
QT.declarative.sources = $$QT_MODULE_BASE/src/declarative
QT.declarative.libs = $$QT_MODULE_LIB_BASE
QT.declarative.plugins = $$QT_MODULE_PLUGIN_BASE
QT.declarative.imports = $$QT_MODULE_IMPORT_BASE
QT.declarative.depends = gui script network opengl xmlpatterns
QT.declarative.DEFINES = QT_DECLARATIVE_LIB

QT_CONFIG += declarative
