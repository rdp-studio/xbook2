X_LIBS		:= libxlibc-0.0.1.a libgapi-0.1.0.a
X_LIBS		+= liblvgl-7.6.0.a

X_CFLAGS    := -fno-builtin -Wall -Wunused -fno-PIE -m32 -fno-stack-protector -std=gnu99

NAME		:= lv
SRC			:= main.c 
SRC			+= examples/lv_demo_widgets/

define CUSTOM_TARGET_CMD
echo [APP] $@; \
$(LD) $(X_LDFLAGS) $(X_OBJS) -o $@ $(patsubst %, -L%, $(X_LIBDIRS)) --start-group $(patsubst %, -l:%, $(X_LIBS)) --end-group; \
cp $@ $(srctree)/../develop/rom/bin
endef
