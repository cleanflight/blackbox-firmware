all : blackbox_bench

OPTIMIZE = -O3 
CFLAGS = -g3 $(OPTIMIZE)
LDFLAGS = -flto $(OPTIMIZE)

blackbox_bench: obj/blackbox_bench

obj/blackbox_bench : obj/blackbox_bench.o obj/serial.o
	$(CC) -o $@ $^ $(LDFLAGS)

obj/%.o : src/%.c
	@mkdir -p $(dir $@)
	$(CC) -c -o $@ $(CFLAGS) $<

clean :
	rm -f obj/*