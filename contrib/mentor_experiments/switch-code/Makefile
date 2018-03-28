# To demonstrate, do:  make check    [Checkpoints every 5 seconds]

# The name will be the same as the current directory name.
NAME=${shell basename $$PWD}

# By default, your resulting library will have this name.
LIBNAME=dmtcp_${NAME}

# As you add new files to your hijack library, add the object file names here.
LIBOBJS = ${NAME}.o

# Modify if your DMTCP_ROOT is located elsewhere.
ifndef DMTCP_ROOT
  DMTCP_ROOT=../../../
endif
DMTCP_INCLUDE=${DMTCP_ROOT}/include
JALIB_INCLUDE=${DMTCP_ROOT}/jalib/

CFLAGS += -I${DMTCP_INCLUDE} -I${JALIB_INCLUDE} -DDMTCP -fPIC -c --std=gnu99 -g -O0
CXXFLAGS += -I${DMTCP_INCLUDE} -I${JALIB_INCLUDE} -I${DMTCP_ROOT}/src/ -DDMTCP -fPIC -c -g -O0

# enable this for debugging the plugin
# CFLAGS += -DCKPTTIMER_PLUGIN_DEBUG
# CXXFLAGS += -DCKPTTIMER_PLUGIN_DEBUG

DEMO_PORT=7781

default: ${LIBNAME}.so

check: tidy ${LIBNAME}.so dmtcp1 libdmtcp1.so
	@# Note that full path of plugin (using $$PWD in this case) is required.
	@DEBUG_LIB=./libdmtcp1.so REPLACE_SYMBOL_WITH_DEBUG=_ZN13switch_plugin6MyTime9time_loopEv \
  ${DMTCP_ROOT}/bin/dmtcp_launch --port ${DEMO_PORT}\
	  --with-plugin $$PWD/${LIBNAME}.so ./dmtcp1 && \
  echo "Restarting..." && \
  ${DMTCP_ROOT}/bin/dmtcp_restart --port ${DEMO_PORT} \
    ckpt*.dmtcp

dmtcp1-opt.o: dmtcp1.cpp
	${CXX} -I${DMTCP_INCLUDE} -fPIC -c -g -O2 -o $@ $^

dmtcp1: dmtcp1-opt.o
	${CXX} -export-dynamic -fPIC -o $@ $^

dmtcp1-unopt.o: dmtcp1.cpp
	${CXX} -I${DMTCP_INCLUDE} -DDEBUG -fPIC -c -g -O0 -o $@ $^

libdmtcp1.so: dmtcp1-unopt.o
	${CXX} -shared -o $@ $^

# We link the library using C++ for compatibility with the main libdmtcp.so
${LIBNAME}.so: ${LIBOBJS}
	${CXX} -shared -fPIC -o $@ $^ -lrt

.c.o:
	${CC} ${CFLAGS} -o $@ $<
.cpp.o:
	${CXX} ${CXXFLAGS} -o $@ $<

tidy:
	rm -f *~ .*.swp dmtcp_restart_script*.sh ckpt_*.dmtcp

clean:
	rm -f ${LIBOBJS} ${LIBNAME}.so *.o *.so dmtcp1

distclean: clean
	rm -f ${LIBNAME}.so *~ .*.swp dmtcp_restart_script*.sh ckpt_*.dmtcp

dist: distclean
	dir=`basename $$PWD`; cd ..; \
	  tar czvf $$dir.tar.gz --exclude-vcs ./$$dir
	dir=`basename $$PWD`; ls -l ../$$dir.tar.gz

.PHONY: default clean dist distclean
