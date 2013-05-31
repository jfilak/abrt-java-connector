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
	rpmbuild $(RPM_DIRS) -ba $(PKG_DIR)/abrt-java-connector.spec

# Make sure the output dir is created
$(OUT_DIR):
	mkdir -p $@ && cd $@ && cmake ../

.PHONY: clean
clean:
	rm -rf $(OUT_DIR)
