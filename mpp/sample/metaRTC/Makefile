include ../Makefile.param

# INC_FLAGS :=-I./hisivenc/include
INC_FLAGS +=-I./hisivenc
INC_FLAGS +=-I./hisivenc/adp
INC_FLAGS +=-I./include
INC_FLAGS +=-I$(INC_DIR)/opus
# INC_FLAGS +=-I../../include

RTMP_LIBS=./thirdparty/lib/himix200/libssl.a \
	./thirdparty/lib/himix200/libcrypto.a \
	./thirdparty/lib/himix200/libyuv.a \
	./thirdparty/lib/himix200/libopus.a \
	./thirdparty/lib/himix200/libusrsctp.a \
	./thirdparty/lib/himix200/libsrtp2.a \
	./meta_lib/libmetartc6.a \
	./meta_lib/libmetartccore6.a \

# SRCS := $(wildcard *.cpp)
SRCS := $(wildcard *.c)
SRCS += $(wildcard ./hisivenc/*.c)
SRCS += $(wildcard ./hisivenc/adp/*.c)

OBJS := main.cpp.o YangRtcPublish.cpp.o loto_BufferManager.cpp.o
# OBJS += $(SRCS:%.c,%.o)
OBJS += $(patsubst %.c, %.o, $(SRCS))
# OBJS += $(SRCS:%.cpp,%.cpp.o) 

TARGET := loto_metartc
# CFLAGS += $(INC_FLAGS)
CFLAGS += -L$(REL_LIB) -lsecurec -Wl,-rpath=/usr/lib

all:$(TARGET)

$(TARGET):$(OBJS)
	@$(CXX) $(CFLAGS) -pthread -lm -o $@ $^ -Wl,--start-group $(MPI_LIBS) $(SENSOR_LIBS) $(AUDIO_LIBA) $(REL_LIB)/libsecurec.a $(RTMP_LIBS) -Wl,--end-group

%.cpp.o:%.cpp
	$(CXX) $(CFLAGS) -c -o $@ $<

%.o:%.c
	$(CXX) $(CFLAGS) -c -o $@ $<

.PHONY : clean echo
clean:
	@rm -f $(TARGET)
	@rm -f $(OBJS)

echo:
	@echo $(CFLAGS)
	@echo
	@echo $(OBJS)
	@echo
	@echo $(CC)
	@echo
	@echo $(CXX)
	@echo