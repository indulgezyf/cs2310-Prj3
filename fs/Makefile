# EXES = FS FS_local FC test_fs

# BUILD_DIR = build

# FS_OBJS = src/server.o \
# 	src/block.o \
# 	src/fs.o \
# 	src/inode.o 

# FS_local_OBJS = src/main.o \
# 	src/block.o \
# 	src/fs.o \
# 	src/inode.o

# FC_OBJS = src/client.o

# test_fs_OBJS = tests/main.o \
# 	src/block.o \
# 	src/fs.o \
# 	src/inode.o \
# 	tests/test_block.o \
# 	tests/test_fs.o \
# 	tests/test_inode.o

# # Add $(BUILD_DIR) to the beginning of each object file path
# $(foreach exe,$(EXES), \
#     $(eval $(exe)_OBJS := $$(addprefix $$(BUILD_DIR)/,$$($(exe)_OBJS))))

# LIB_SRCS = ../lib/tcp_buffer.c ../lib/tcp_utils.c ../lib/thpool.c
# # Replace .. with $(BUILD_DIR)
# LIB_OBJS = $(LIB_SRCS:../lib/%.c=$(BUILD_DIR)/lib/%.o)

# CC ?= gcc
# CFLAGS += -Wall -MMD -Iinclude -I../include
# LDFLAGS += -lpthread

# DEBUG ?= 1
# ifeq ($(DEBUG),1)
# CFLAGS += -fsanitize=address -g
# endif

# all: $(EXES)

# define make-target
# $1: $(LIB_OBJS) $$($1_OBJS)
# 	$$(CC) $$(CFLAGS) -o $$@ $$^ $$(LDFLAGS)
# endef

# $(foreach exe,$(EXES),$(eval $(call make-target,$(exe))))

# run: FS_local
# 	sudo sysctl vm.mmap_rnd_bits=28
# 	./FS_local

# test: test_fs
# 	sudo sysctl vm.mmap_rnd_bits=28
# 	./test_fs

# # rules to build object files
# $(BUILD_DIR)/%.o: %.c
# 	@mkdir -p $(@D)
# 	$(CC) $(CFLAGS) -c $< -o $@

# # rules to build library object files
# $(BUILD_DIR)/lib/%.o: ../lib/%.c
# 	@mkdir -p $(@D)
# 	$(CC) $(CFLAGS) -c $< -o $@

# clean:
# 	rm -rf $(BUILD_DIR) $(EXES)

# OBJS = $(foreach exe,$(EXES),$($(exe)_OBJS)) $(LIB_OBJS)
# DEPS = $(OBJS:.o=.d)
# -include $(DEPS)

# .PHONY: all clean run test

# Makefile for FS (server), FC (client), test_fs (tests)

# Makefile for FS (server) and FC (client)

EXES = FS FC

BUILD_DIR = build

# ----------------------------
# 源文件对象列表（无 build 前缀）
# ----------------------------

FS_OBJS = \
	src/server.o \
	src/block.o \
	src/fs.o \
	src/inode.o \
	src/session.o \
	src/handle.o \
	 src/dirop.o

FC_OBJS = \
	src/client.o

# 在每个对象名前面加上 build 目录前缀
$(foreach exe,$(EXES), \
	$(eval $(exe)_OBJS := $$(addprefix $$(BUILD_DIR)/,$$($(exe)_OBJS))))

# ----------------------------
# 第三方库对象
# ----------------------------

LIB_SRCS = \
	../lib/tcp_buffer.c \
	../lib/tcp_utils.c \
	../lib/thpool.c

LIB_OBJS = $(LIB_SRCS:../lib/%.c=$(BUILD_DIR)/lib/%.o)

# ----------------------------
# 编译器设置
# ----------------------------

CC   ?= gcc
CFLAGS += -Wall -MMD -Iinclude -I../include -Isrc
LDFLAGS += -lpthread

DEBUG ?= 1
ifeq ($(DEBUG),1)
	CFLAGS += -fsanitize=address -g
endif

# ----------------------------
# 默认目标
# ----------------------------

all: $(EXES)

# ----------------------------
# 定义可执行文件规则
# ----------------------------

define make-target
$1: $(LIB_OBJS) $$($1_OBJS)
	$(CC) $(CFLAGS) -o $$@ $$^ $(LDFLAGS)
endef

$(foreach exe,$(EXES),$(eval $(call make-target,$(exe))))

# ----------------------------
# 运行目标
# ----------------------------

run-server: FS
	./FS

run-client: FC
	./FC

# ----------------------------
# 对象文件生成规则
# ----------------------------

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/lib/%.o: ../lib/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# ----------------------------
# 清理
# ----------------------------

clean:
	rm -rf $(BUILD_DIR) $(EXES)

# ----------------------------
# 自动依赖
# ----------------------------

OBJS = $(foreach exe,$(EXES),$($(exe)_OBJS)) $(LIB_OBJS)
DEPS = $(OBJS:.o=.d)
-include $(DEPS)

.PHONY: all clean run-server run-client
