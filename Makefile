#
# Makefile for compiling Test.java and also JVM TI agent native library.
#
# Pavel Tisnovsky <ptisnovs@redhat.com>
#



# Useful .bashrc aliases for us lazy Java developers:
# alias m=make
# alias b=make build
# alias r=make run
# alias c=make clean



OUT_DIR=bin
PKG_DIR=package

all: run

build: $(OUT_DIR)
	cd $(OUT_DIR) && make

builddebug:
	$(MAKE) distclean build CMAKE_OPTS="-DCMAKE_BUILD_TYPE=Debug"

.PHONY: run
run: build
	cd $(OUT_DIR) && make run

.PHONY: dist
dist: $(OUT_DIR)
	cd $(OUT_DIR) && make dist

RPM_DIRS = --define "_sourcedir `pwd`/$(OUT_DIR)" \
		--define "_rpmdir `pwd`/$(OUT_DIR)" \
		--define "_specdir `pwd`/$(PKG_DIR)" \
		--define "_builddir `pwd`/$(OUT_DIR)" \
		--define "_srcrpmdir `pwd`/$(OUT_DIR)"

.PHONY: rpm
rpm: dist
	sed -e 's/global commit .*$$/global commit '"$$(git log -1 --format=%H)"'/' \
		-e 's/%{?dist}/.'"$$(git log -1 --format=%h)%{?dist}"'/' \
		$(PKG_DIR)/abrt-java-connector.spec > $(OUT_DIR)/abrt-java-connector.spec && \
	rpmbuild $(RPM_DIRS) $(RPM_FLAGS) -ba $(OUT_DIR)/abrt-java-connector.spec

# Make sure the output dir is created
$(OUT_DIR):
	mkdir -p $@ && cd $@ && cmake $$CMAKE_OPTS ../

.PHONY: clean
clean:
	cd $(OUT_DIR) && make clean

.PHONY: distclean
distclean:
	rm -rf $(OUT_DIR)

.PHONY: check
check: build
	if [ "_0" != "_$$(id -u)" ]; then cd $(OUT_DIR) && make test; else echo "Cannot run tests under root user."; exit 1; fi
