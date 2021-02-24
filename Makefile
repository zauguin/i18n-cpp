LLVM_CONFIG = llvm-config
LLVM_CONFIG.LDLIBS = -lclang-cpp $(shell $(LLVM_CONFIG) $(LLVM_CONFIGFLAGS) --libs)
LLVM_CONFIG.LDFLAGS = $(shell $(LLVM_CONFIG) $(LLVM_CONFIGFLAGS) --ldflags)
LLVM_CONFIG.CXXFLAGS := $(shell $(LLVM_CONFIG) $(LLVM_CONFIGFLAGS) --cxxflags) -std=c++20
# For CXXFLAGS specify -std= *after* the llvm flags to overwrite the language standard

CXX = clang++
CXXFLAGS += -std=c++20 -MMD
LDFLAGS += -fuse-ld=lld

SRCS = clang/action.cpp clang/plugin.cpp clang/attr.cpp clang/tool.cpp lib/write_po.cpp merge/main.cpp merge/messages.cpp
OBJS = $(SRCS:.cpp=.o)

.PHONY: all clean
all: clang/tool clang/i18n.so merge/merge_pot
clean:
	rm -f $(OBJS:.o=.d) $(OBJS) clang/tool clang/i18n.so merge/merge_pot

clang/plugin.o clang/attr.o lib/write_po.o: CXXFLAGS += -fPIC

clang/tool clang/i18n.so: LDFLAGS += $(LLVM_CONFIG.LDFLAGS)
clang/tool: LDLIBS += $(LLVM_CONFIG.LDLIBS)

clang/action.o clang/attr.o clang/tool.o clang/plugin.o: CXXFLAGS += $(LLVM_CONFIG.CXXFLAGS)

clang/tool: clang/action.o clang/attr.o clang/tool.o lib/write_po.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

clang/i18n.so: clang/plugin.o clang/attr.o lib/write_po.o
	$(LINK.cc) -shared $^ $(LOADLIBES) -o $@

merge/merge_pot: merge/main.o merge/messages.o lib/write_po.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

-include $(OBJS:.o=.d)
