# Target library
lib  := libfs.a
objs := disk.o fs.o

# Compilation toolchain
CC   := gcc

# General gcc options
CCFLAGS := -Wall -Werror

# Print commands only if explicitly requested
ifneq ($(V),1)
Q = @
endif

# Debug flags
ifneq ($(D),1)
CCFLAGS	+= -O2
else
CCFLAGS	+= -O0
CCFLAGS	+= -g
endif

all: $(lib)

# Automatic dependency traking
deps := $(patsubst %.o,%.d,$(objs))
-include $(deps)
DEPFLAGS = -MMD -MF $(@:.o=.d)

# Compiling library
$(lib): $(objs)
	@echo "AR $@"
	$(Q)ar rcs $@ $^

# Compiling objects
%.o: %.c %.h
	@echo "CC $@"
	$(Q)$(CC) $(CCFLAGS) -c -o $@ $< $(DEPFLAGS)

# Cleaning
clean:
	@echo "CLEAN"
	$(Q)rm -rf $(objs) $(lib) $(deps)
