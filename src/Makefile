CHMSEE_ROOTDIR = ..
COMPONENTSDIR = ${CHMSEE_ROOTDIR}/components

TARGET = ${COMPONENTSDIR}/libxpcomchm.so

SRCS = csChm.cpp csChmModule.cpp csChmfile.c
OBJS = csChm.o csChmModule.o csChmfile.o

INTERFACE = csIChm
IDL = ${INTERFACE}.idl
XPT = ${COMPONENTSDIR}/xpcomchm.xpt

LIBXUL_SDK = /usr/pkg/lib/xulrunner-sdk
SDK_IDL = ${LIBXUL_SDK}/idl
XPIDL = ${LIBXUL_SDK}/bin/xpidl
XPT_LINK = ${LIBXUL_SDK}/bin/xpt_link

MOZ_DEBUG_DISABLE_DEFS	= -DNDEBUG -DTRIMMED

VISIBILITY_FLAGS = -fvisibility=hidden
INCLUDES         = -I/usr/pkg/include -I/usr/include -I. -I${LIBXUL_SDK}/include
DEFINES		 = -fno-rtti -fno-exceptions -Wall -Wpointer-arith -Woverloaded-virtual \
		   -Wsynth -Wno-ctor-dtor-privacy -Wno-non-virtual-dtor -Wcast-align -Wno-invalid-offsetof -Wno-variadic-macros \
		   -O2 -fno-strict-aliasing -Dunix -fshort-wchar -pthread -pipe
LIBXUL_CXXFLAGS  = -DMOZILLA_CLIENT -include mozilla-config.h

CFLAGS          += ${VISIBILITY_FLAGS} ${INCLUDES}
CXXFLAGS        += ${VISIBILITY_FLAGS} ${INCLUDES} ${DEFINES} ${LIBXUL_CXXFLAGS}


XPCOM_FROZEN_LDOPTS = -Wl,-R${LIBXUL_SDK}/bin -L${LIBXUL_SDK}/bin -lxpcom -lmozalloc
NSPR_LIBS           = -Wl,-R${LIBXUL_SDK}/lib -L${LIBXUL_SDK}/lib -lplds4 -lplc4 -lnspr4 -L/usr/pkg/lib -L/usr/lib -pthread
CHM_LIB             = -Wl,-R/usr/pkg/lib -L/usr/pkg/lib -lchm

LDFLAGS            += ${INCLUDES} \
		      ${DEFINES} \
		      ${MOZ_DEBUG_DISABLE_DEFS} \
		      -O2 -fPIC -DPIC -shared -Wl,-soname,${TARGET} -lpthread \
		      ${LIBXUL_SDK}/lib/libxpcomglue_s.a \
		      ${XPCOM_FROZEN_LDOPTS} \
		      ${NSPR_LIBS} \
		      ${CHM_LIB}


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
