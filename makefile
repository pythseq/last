CXXFLAGS = -msse4 -O3 -std=c++11 -pthread -DHAS_CXX_THREADS
all:
	@cd src && $(MAKE) CXXFLAGS="$(CXXFLAGS)"

progs = src/lastdb src/lastal src/last-split src/last-merge-batches	\
src/last-pair-probs src/lastdb8 src/lastal8 src/last-split8

prefix = /usr/local
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
install: all
	mkdir -p $(bindir)
	cp $(progs) scripts/* $(bindir)

clean:
	@cd src && $(MAKE) clean

docs: doc/last-matrices.rst doc/last-seeds.rst

doc/last-matrices.rst: data/*.mat
	./build/mat-doc.sh data/*.mat > $@

doc/last-seeds.rst: data/*.seed
	cd data && ../build/seed-doc.sh [!R]*d RY?-*d RY??-*d > ../$@

tag:
	git tag -m "" `git rev-list HEAD | grep -c .`
