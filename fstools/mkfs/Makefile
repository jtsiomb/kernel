ksrc = ../../src

obj = mkfs.o fs.o
dep = $(obj:.o=.d)
bin = mkfs

CC = gcc
CFLAGS = -pedantic -Wall -g -I$(ksrc)

$(bin): $(obj)
	$(CC) -o $@ $(obj) $(LDFLAGS)

-include $(dep)

fs.o: $(ksrc)/fs.c
	$(CC) $(CFLAGS) -c $< -o $@

%.d: %.c
	@$(CPP) $(CFLAGS) $< -MM -MT $(@:.d=.o) >$@

.PHONY: clean
clean:
	rm -f $(obj) $(bin) $(dep)
