# Target library
lib := libfs.a
objs := disk.o fs.o
CC	:= gcc
CFLAGS	:= -Wall -Werror

all: $(lib)

deps := $(patsubst %.o,%.d,$(objs))
-include $(deps)
DEPFLAGS = -MMD -MF $(@:.o=.d)

$(lib): $(objs)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< $(DEPFLAGS)

clean:
	rm -f $(lib) $(objs) *.d
## TODO: Phase 1
