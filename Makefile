CUDA_VER?=11.1
ifeq ($(CUDA_VER),)
	$(error "CUDA_VER" is not set)
endif

APP:= yolov4_detector

TARGET_DEVICE = $(shell gcc -dumpmachine | cut -f1 -d -)

CC:= g++

NVDS_VERSION:= 5.1

LIB_INSTALL_DIR?=/opt/nvidia/deepstream/deepstream-$(NVDS_VERSION)/lib/
APP_INSTALL_DIR?=/opt/nvidia/deepstream/deepstream-$(NVDS_VERSION)/bin/

SRCS:= $(wildcard src/*.cpp)

INCS:= $(wildcard src/*.hpp)

PKGS:= gstreamer-1.0 opencv4

CFLAGS+= $(shell pkg-config --cflags $(PKGS))
CFLAGS+=-I /opt/nvidia/deepstream/deepstream-$(NVDS_VERSION)/sources/includes \
		-I /usr/local/cuda-$(CUDA_VER)/include


LIBS+= $(shell pkg-config --libs $(PKGS))
LIBS+= -L$(LIB_INSTALL_DIR) -L/usr/local/cuda/lib64 -lcudart \
	   -lnvdsgst_meta -lnvds_meta -lnvdsgst_helper -lm -lrt \
       -Wl,-rpath,$(LIB_INSTALL_DIR)
LIBS+= -pthread -O3 -Ofast
LIBS+= -lcurl -lgnutls -luuid -lnvbufsurface -lnvbufsurftransform
LIBS+=  -lopencv_core -lopencv_highgui -lopencv_imgproc -lboost_system -lopencv_imgcodecs -pthread -lz -lssl -lcrypto -lboost_program_options \
		-lboost_filesystem -lboost_date_time -lboost_context -lboost_coroutine -lboost_chrono \
		-lboost_log -lboost_thread -lboost_log_setup -lboost_regex -lboost_atomic
		

OBJS:= $(SRCS:.cpp=.o)

all: $(APP) yolov4

%.o: %.cpp $(INCS) 
	$(CC) -c $(CFLAGS) $< -o $@

yolov4:
	cd custom_parsers/nvds_customparser_yolov4 && $(MAKE)

$(APP): $(OBJS) Makefile
	$(CC) $(OBJS) $(LIBS) -o $(APP)

clean:
	rm -rf $(OBJS) $(APP)
	cd custom_parsers/nvds_customparser_yolov4 && $(MAKE) clean