include ./make.config

all: bin/hello-static bin/hello-dynamic bin/hello

bin/hello-static: examples/hello.c
	$(CC) $(CFLAGS) $(FLAGS_OPENMP) $< -o $@

lib/liboar.so: src/oar.c
	$(CC) $(CFLAGS) $(OPENMP_INCLUDE) $< -shared -fPIC -o $@

bin/hello-dynamic: examples/hello.c lib/liboar.so
	$(CC) $(CFLAGS) $(FLAGS_OPENMP) -Llib/ -Wl,--no-as-needed -loar -Wl,--as-needed -Wl,--rpath,. $< -o $@

bin/hello: examples/hello.c
	$(CC) $(CFLAGS) $(FLAGS_OPENMP) $< -o $@

.PHONY:all clean

clean:
	$(RM) bin/hello-static bin/hello-dynamic bin/hello lib/liboar.so *~


