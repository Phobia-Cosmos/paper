CC = clang
SRCS = $(wildcard *.c)
# 头文件搜索路径; 当前目录; 允许 #include "memtools.h"
INCPATH = -I./
BIN = ijmp_ijmp jcc_ijmp ret_ijmp ooo_ijmp btc-djmp_ijmp btc-ijmp_ijmp
OBJS = $(filter-out $(BIN:=.o), $(SRCS:.c=.o))
# 共享一份公共工具代码 memtools.c
OBJS = memtools.o
CFLAGS = -O2 -Wall -Wno-language-extension-token -Wno-unused-function -pedantic -Wstrict-prototypes -Wdeprecated-non-prototype -DARCH=$(shell ./arch.sh)
# 保存头文件依赖;支持增量构建
DEP_FILE = .depend

.PHONY = all clean

all: depend $(BIN)

$(BIN: % = %.o): %: %.o
# ijmp_ijmp: ijmp_ijmp.o
# jcc_ijmp: jcc_ijmp.o
# ...


# $@ → 当前目标（如 ijmp_ijmp）
# $^ → 所有依赖（.o + memtools.o）
$(BIN): $(OBJS)
	@echo LD $@
	@$(CC) $^ $(LDFLAGS) $(LIBS) -o $@

# $<：第一个依赖（源文件）
# 编译但不链接
# 每个 .c 都会生成自己的 .o
.c.o:
	@echo CC $<
	@$(CC) -c $(CFLAGS) $(INCPATH) $<

clean:
	@echo CLEAN
	@$(RM) $(OBJS)
	@$(RM) $(DEP_FILE)
	@$(RM) $(BIN)
	@$(RM) $(foreach f,$(BIN),$f.o)

depend: $(DEP_FILE)
	@touch $(DEP_FILE)

# -E -MM：只生成依赖关系
$(DEP_FILE):
	@echo DEP $@
	@-$(CC) -E -MM $(CFLAGS) $(INCPATH) $(SRCS) >> $(DEP_FILE)

ifeq (,$(findstring clean,$(MAKECMDGOALS)))
-include $(DEP_FILE)
endif
