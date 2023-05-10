VER_MAJOR:=1
VER_MINOR:=4
VER_BUILD:=4
VER_EXTEN:=3
BIN_VERSION:=$(VER_MAJOR).$(VER_MINOR).$(VER_BUILD).$(VER_EXTEN)

include ../Makefile.param

SRCS:=$(wildcard *.c)
SRCS+=$(wildcard audio_adp/*.c)
OBJS:=$(SRCS:%.c=%.o)
TARGET:=loto_rtmp

INC_FLAGS+=-I./audio_adp
INC_FLAGS+=-I../librtmp
INC_FLAGS+=-I$(INC_DIR)/opus
INC_FLAGS+=-I$(INC_DIR)/fdk-aac

RTMP_LIBS+=../librtmp/librtmp.a
RTMP_LIBS+=../openssl-1.0.1p/arm/lib/libssl.a
RTMP_LIBS+=../openssl-1.0.1p/arm/lib/libcrypto.a 
RTMP_LIBS+=../zlib-1.2.8/arm/lib/libz.a
RTMP_LIBS+=$(LIB_DIR)/fdk-aac/libfdk-aac.a

CFLAGS+=-L$(REL_LIB) -lsecurec -Wl,-rpath=/usr/lib
CFLAGS+=-DBIN_VERSION=\"$(BIN_VERSION)\"

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

.PHONY : clean
all: $(TARGET)

$(TARGET):$(OBJS)
	@$(CC) $(CFLAGS) -lpthread -lm -o $@ $^ -Wl,--start-group $(MPI_LIBS) $(SENSOR_LIBS) $(AUDIO_LIBA) $(REL_LIB)/libsecurec.a $(RTMP_LIBS) -Wl,--end-group

release:
	@mkdir -p ./bin
	@cp $(TARGET) ./bin/$(TARGET)-$(BIN_VERSION)

delete_release:
	@rm -f ./bin/$(TARGET)-$(BIN_VERSION)

clean:
	@rm -f $(TARGET)
	@rm -f $(OBJS)
