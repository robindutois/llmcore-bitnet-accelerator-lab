APP = run-bitlinear
APP_OBJS = run-bitlinear.o

all: build

build: $(APP)

$(APP): $(APP_OBJS)
	$(CC) -o $@ $(APP_OBJS) $(LDFLAGS) $(LDLIBS) -lrt

%.o: %.c
	$(CC) -c -O2 -o $@ $< $(CFLAGS)

clean:
	rm -f $(APP) *.o
