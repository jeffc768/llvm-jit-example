.SUFFIXES:
.SUFFIXES: .cpp .h .o

.PHONY: all clean

# For GCC, include -Wno-class-memaccess
CCFLGS := -Wall -W -Wwrite-strings -Wno-unused-parameter -Wno-missing-braces -Wno-missing-field-initializers -D__STDC_LIMIT_MACROS -fno-strict-aliasing -Wno-register

OBJDIR := Debug
CCFLGS += -g -D_DEBUG $(CXXFLAGS) -std=c++17 -I .
LDFLGS := $(LDFLAGS)
BIFLGS := -v -d -t
FLFLGS :=

# Add filtered LLVM compile and link options.
CCFLGS += -I $(shell $(USE_LLVM) --includedir) -DTARGET_TRIPLE=\"$(shell $(USE_LLVM) --host-target)\"
CCFLGS += $(filter-out -DNDEBUG -D_DEBUG -O2 -O3 -std=c++0x -std=c++11 -g -fomit-frame-pointer -Wmissing-field-initializers -Wno-class-memaccess -W -Wall -D__STDC_LIMIT_MACROS,$(shell $(USE_LLVM) --cxxflags))
LDFLGS += $(filter-out -lffi -ledit,$(shell $(USE_LLVM) --ldflags --system-libs))
LIBS = $(shell $(USE_LLVM) --libs)

SRCS = $(wildcard *.cpp *.l *.y)
OBJS = $(patsubst %,$(OBJDIR)/%.o,$(sort $(basename $(SRCS))))
DEPENDS = $(patsubst %.o,%.d,$(OBJS))
YACCFILES = $(filter %.y,$(SRCS))
FLEXFILES = $(filter %.l,$(SRCS))

# Cancel builtin rules.
%.cpp: %.y
%.h: %.y
%.cpp: %.l
%.o: %.cpp

all: $(OBJDIR)/calc

clean:
	rm -rf $(OBJDIR) $(patsubst %.y,%.h,$(YACCFILES)) $(patsubst %.y,%.cpp,$(YACCFILES)) $(patsubst %.y,%.output,$(YACCFILES)) $(patsubst %.l,%.cpp,$(FLEXFILES))

$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: %.cpp
	@echo Compiling $*.cpp for $(OBJDIR) build
	@if $(CXX) -MD -MT $@ -MP -MF $(OBJDIR)/$*.CXXd -c $(CCFLGS) $< -o $@ ; \
	then mv -f $(OBJDIR)/$*.CXXd $(OBJDIR)/$*.d ; \
	else rm -f $(OBJDIR)/$*.CXXd; exit 1; fi

.PRECIOUS: $(patsubst %.y,%.output,$(YACCFILES))
$(YACCFILES:%.y=$(OBJDIR)/%.o): $(OBJDIR)/%.o: %.cpp
%.cpp %.h: %.y
	@echo Bisoning $*.y
	@bison $(BIFLGS) -o $*.tab.c $< ; \
	mv -f $*.tab.c $*.cpp ; \
	mv -f $*.tab.h $*.h

$(FLEXFILES:%.l=$(OBJDIR)/%.o): $(OBJDIR)/%.o: %.cpp
%.cpp: %.l
	@echo Flexing $*.l
	@flex $(FLFLGS) -o $*.cpp $<

$(OBJDIR)/calc: $(OBJS)
	@echo Linking Calc
	@$(CXX) $(OBJS) $(LIBS) $(LDFLGS) -o $(OBJDIR)/calc

$(OBJS): | $(OBJDIR)

$(OBJDIR)/lexer.o: parser.h
$(OBJDIR)/calc.o: parser.h

-include /dev/null $(DEPENDS)
