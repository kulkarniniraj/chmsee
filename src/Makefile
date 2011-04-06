LIBXUL_SDK = /usr/pkg/lib/xulrunner-sdk

NSPR_INCLUDES = /usr/pkg/include/xulrunner
NSPR_LIBS = -Wl,-R/usr/pkg/lib/xulrunner -pthread -L/usr/pkg/lib/xulrunner -lplds4 -lplc4 -lnspr4

CHMLIB_INCLUDES = /usr/pkg/include
CHMLIB_LIBS = -Wl,-R/usr/pkg/lib -L/usr/pkg/lib -lchm

CHMSEE_ROOTDIR = ..
COMPONENTSDIR = ${CHMSEE_ROOTDIR}/components

TARGET = ${COMPONENTSDIR}/libxpcomchm.so

SRCS = csChm.cpp csChmModule.cpp csChmfile.c
OBJS = csChm.o csChmModule.o csChmfile.o

INTERFACE = csIChm
IDL = ${INTERFACE}.idl
XPT = ${COMPONENTSDIR}/xpcomchm.xpt

SDK_IDL = ${LIBXUL_SDK}/idl
XPIDL = ${LIBXUL_SDK}/bin/xpidl
XPT_LINK = ${LIBXUL_SDK}/bin/xpt_link

MOZ_DEBUG_DISABLE_DEFS	= -DNDEBUG -DTRIMMED

VISIBILITY_FLAGS = -fvisibility=hidden
INCLUDES         = -I/usr/include -I. -I${LIBXUL_SDK}/include -I${NSPR_INCLUDES} -I${CHMLIB_INCLUDES}
DEFINES		 = -fno-rtti -fno-exceptions -Wall -Wpointer-arith -Woverloaded-virtual \
		   -Wsynth -Wno-ctor-dtor-privacy -Wno-non-virtual-dtor -Wcast-align -Wno-invalid-offsetof -Wno-variadic-macros \
		   -O2 -fno-strict-aliasing -Dunix -fshort-wchar -pthread -pipe
LIBXUL_CXXFLAGS  = -DMOZILLA_CLIENT -include mozilla-config.h

CFLAGS          += ${VISIBILITY_FLAGS} ${INCLUDES}
CXXFLAGS        += ${VISIBILITY_FLAGS} ${INCLUDES} ${DEFINES} ${LIBXUL_CXXFLAGS}


XPCOM_FROZEN_LDOPTS = -Wl,-R${LIBXUL_SDK}/bin -L${LIBXUL_SDK}/bin -lxpcom -lmozalloc

LDFLAGS            += ${INCLUDES} \
		      ${DEFINES} \
		      ${MOZ_DEBUG_DISABLE_DEFS} \
		      -O2 -fPIC -DPIC -shared -Wl,-soname,${TARGET} -lpthread \
		      ${LIBXUL_SDK}/lib/libxpcomglue_s.a \
		      ${XPCOM_FROZEN_LDOPTS} \
		      ${NSPR_LIBS} \
		      ${CHMLIB_LIBS}


all: ${TARGET}

${XPT}: ${IDL}
	${XPIDL} -w -v -m header -I ${SDK_IDL} ${IDL}
	${XPIDL} -w -v -m typelib -I ${SDK_IDL} ${IDL}
	${XPT_LINK} ${XPT} ${INTERFACE}.xpt

${TARGET}: ${XPT} ${OBJS}
	${CC} ${OBJS} -o ${TARGET} ${LDFLAGS}

%.o: %.c
	${CC} ${CFLAGS} -c $<

%.o: %.c++
	${CXX} ${CXXFLAGS} -c $<

clean:
	rm ${TARGET} ${OBJS} ${XPT}
