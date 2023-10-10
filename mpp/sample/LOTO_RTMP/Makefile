VER_MAJOR:=1
VER_MINOR:=8
VER_BUILD:=5
BIN_VERSION:=$(VER_MAJOR).$(VER_MINOR).$(VER_BUILD)
RELEASE_VERSION:=$(VER_MAJOR).$(VER_MINOR)

include ../Makefile.param

# c source files
C_SRCS:=$(wildcard *.c)
C_SRCS+=$(wildcard audio_adp/*.c)

# c object files
C_OBJS:=$(patsubst %.c, %.o, $(C_SRCS))

# cpp source files
CPP_SRCS:=$(wildcard *.cpp)

# cpp object files
CPP_OBJS:=$(patsubst %.cpp, %.o, $(CPP_SRCS))

TARGET:=loto_rtmp

INC_FLAGS+=-I./audio_adp
INC_FLAGS+=-I$(INC_DIR)

# opus
INC_FLAGS+=-I$(INC_DIR)/opus

# librtmp
INC_FLAGS+=-I../librtmp
RTMP_LIBS+=../librtmp/librtmp.a

# zlib-1.2.8
INC_FLAGS+=-I$(INC_DIR)/zlib
RTMP_LIBS+=$(LIB_DIR)/zlib/libz.a

# openssl-1.0.1p
INC_FLAGS+=-I$(INC_DIR)/openssl-1.0.1p
RTMP_LIBS+=$(LIB_DIR)/openssl-1.0.1p/libssl.a
RTMP_LIBS+=$(LIB_DIR)/openssl-1.0.1p/libcrypto.a

# curl-8.1.0
# INC_FLAGS+=-I$(INC_DIR)/curl-8.1.0
# RTMP_LIBS+=$(LIB_DIR)/curl-8.1.0/libcurl.a

CFLAGS+=-L$(REL_LIB) -lsecurec -Wl,-rpath=/usr/lib
# CFLAGS+=-DBIN_VERSION=\"$(BIN_VERSION)\"

OSD_DEV_INFO_NOT?=n # y: no osd, n: display
ROTATION_90?=y
VPSS_CROP?=n
FISHEYE_GDC?=

# don't add osd of timestamp and device number
ifeq ($(OSD_DEV_INFO_NOT), y)
CFLAGS+=-DOSD_DEV_INFO_NOT
endif

# timewise rotation 90 degree
ifeq ($(ROTATION_90), y)
CFLAGS+=-DVPSS_ROTATION_90
endif

# only add crop in VPSS
ifeq ($(VPSS_CROP), y)
CFLAGS+=-DVPSS_CROP
endif

# add GDC
ifeq ($(FISHEYE_GDC), y)
# CFLAGS += -DVPSS_CROP
CFLAGS+=-DFISHEYE_GDC
GDC_LIBA:=$(REL_LIB)/libhifisheyecalibrate.a
# RTMP_LIBS+=$(GDC_LIBA)
endif

# CFLAGS+=-std=c++11

.PHONY : clean
all: $(TARGET)

$(TARGET):$(C_OBJS) $(CPP_OBJS)
	@$(CC) $(CFLAGS) -o $@ $^ -Wl,--start-group $(MPI_LIBS) $(SENSOR_LIBS) $(AUDIO_LIBA) $(REL_LIB)/libsecurec.a $(RTMP_LIBS) -Wl,--end-group

%.o: %.c
	@$(CC) $(CFLAGS) -c -o $@ $<

# %.o: %.cpp
# 	@$(CXX) $(CFLAGS) -c -o $@ $<

release:
	@mkdir -p ./bin
	@cp $(TARGET) ./bin/$(TARGET)-$(BIN_VERSION)
	@cp $(TARGET) ../../../rtmp_pack/kit/root/$(TARGET)

delete_release:
	@rm -f ./bin/$(TARGET)-$(BIN_VERSION)

rebuild_librtmp:
	make -C ../librtmp clean
	make -C ../librtmp

build_with_librtmp: rebuild_librtmp clean all

clean:
	@rm -f $(TARGET)
	@rm -f $(C_OBJS)
	@rm -f $(CPP_OBJS)

echo:
	@echo $(CFLAGS)
	@echo
	@echo $(C_OBJS)
	@echo
	@echo $(CPP_OBJS)
	@echo
	@echo $(CC)
	@echo
	@echo $(CXX)
	@echo
