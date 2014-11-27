
SRCFILES=wakeup.c

all: clean wakeup

wakeup: $(SRCFILES)
	mipsel-linux-android-gcc -o $@ $(SRCFILES)

clean:
	rm -rf wakeup
