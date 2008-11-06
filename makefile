all: version log
	@cd src && $(MAKE)

dist: version log
	mkdir -p last-`svnversion .`/src/CA_code
	mkdir -p last-`svnversion .`/scripts
	mkdir -p last-`svnversion .`/examples
	mkdir -p last-`svnversion .`/doc
	ln src/*.hh src/*.cc src/makefile last-`svnversion .`/src
	ln src/CA_code/*.h src/CA_code/*.c last-`svnversion .`/src/CA_code
	ln scripts/*.sh scripts/*.py last-`svnversion .`/scripts
	ln examples/* last-`svnversion .`/examples
	ln doc/*.txt last-`svnversion .`/doc
	ln *.txt last-`svnversion .`
	zip -qrm archive/last-`svnversion .` last-`svnversion .`

version: FORCE
	echo \"`svnversion .`\" > src/version.hh

log: FORCE
	svn log > ChangeLog.txt

FORCE:
