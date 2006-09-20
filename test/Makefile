# host compiler and flags
HOSTCC		= gcc
HOSTCFLAGS	= -O2
SH4CC		= sh-elf-gcc
SH4INC          = -Iinclude
SH4CFLAGS	= $(SH4INC) -D_arch_dreamcast -D_arch_sub_pristine \
                  -O2 -fno-builtin -fno-strict-aliasing -ml -m4-single-only \
                  -fno-optimize-sibling-calls
SH4LD		= sh-elf-ld
SH4LDFLAGS      = -fno-builtin -fno-strict-aliasing  -ml -m4-single-only \
                  -Wl,-Tdc.x -nostartfiles -nostdlib -Llib
SH4LIBS         = -Wl,--start-group -lc -lgcc -lm -Wl,--end-group
SH4OBJCOPY      = sh-elf-objcopy -O binary
ARMCC		= arm-elf-gcc
ARMCFLAGS	= -O2
ARMLD		= arm-elf-ld
ARMOBJCOPY	= arm-elf-objcopy

BFDLIB		= -L/usr/local/sh-elf-bfd/lib -lbfd -liberty
BFDINCLUDE	= /usr/local/sh-elf-bfd/include
RUNTEST		= ../src/lxdream -c ./lxdream.rc -h -q -p


# cygwin
# these must point to your sh-elf bfd, not the system one
#BFDLIB		= -L/usr/local/lib -lbfd -liberty -lintl
#BFDINCLUDE	= /usr/local/include

#SHARED_OBJECTS	= crt0.so dcload-syscall.so dcload-syscalls.so memcpy.so lib.so
SHARED_OBJECTS	=

%.ao: %.c
	$(ARMCC) $(ARMCFLAGS) -o $@ -c $< 

%.ao: %.s
	$(ARMCC) $(ARMCFLAGS) -o $@ -c $< 

%.so: %.c
	$(SH4CC) $(SH4CFLAGS) -o $@ -c $< 

%.so: %.ac
	$(SH4CC) $(SH4CFLAGS) -o $@ -c $< 

%.so: %.s
	$(SH4CC) $(SH4CFLAGS) -o $@ -c $< 

%.so: %.S
	$(SH4CC) $(SH4CFLAGS) -o $@ -c $< 

%.o: %.c
	$(HOSTCC) $(HOSTCFLAGS) -o $@ -c $<

%.arm: %.ao arm_crt0.ao
	$(ARMCC) -Wl,-Ttext,0x00000000 -nostartfiles -nostdlib -e reset -o $@ arm_crt0.ao $< -lgcc


%.bin: %.arm
	$(ARMOBJCOPY) -O binary $< $@

%.aso: %.bin bin2c
	./bin2c $< $<.c
	$(SH4CC) $(SH4CFLAGS) -o $@ -c $<.c

all: build-tests

test: build-tests
	$(RUNTEST) testmath
	$(RUNTEST) testide -d ../disc/test.nrg
	$(RUNTEST) testta < testta.data < testta2.data < testta3.data < testta4.data < testta5.data
	$(RUNTEST) testregs

build-tests: testmath testide testta testregs

testsh4: crt0.so sh4/testsh4.so sh4/timer.so sh4/add.so
	$(SH4CC) $(SH4LDFLAGS) $^ -o $@ $(SH4LIBS)

testide: crt0.so testide.so ide.so
	$(SH4CC) $(SH4LDFLAGS) $^ -o $@ $(SH4LIBS)

testmath: crt0.so $(SHARED_OBJECTS) testmath.so math.so
	$(SH4CC) $(SH4LDFLAGS) $^  -o $@ $(SH4LIBS)

testregs: crt0.so $(SHARED_OBJECTS) testregs.so
	$(SH4CC) $(SH4LDFLAGS) $^  -o $@ $(SH4LIBS)
	$(SH4OBJCOPY) testregs testregs.bin

testta: crt0.so $(SHARED_OBJECTS) testta.so pvr.so dmac.so asic.so lib.so testdata.so
	$(SH4CC) $(SH4LDFLAGS) $^  -o $@ $(SH4LIBS)
	$(SH4OBJCOPY) testta testta.bin

testrend: crt0.so $(SHARED_OBJECTS) testrend.so pvr.so dmac.so asic.so lib.so testdata.so
	$(SH4CC) $(SH4LDFLAGS) $^  -o $@ $(SH4LIBS)
	$(SH4OBJCOPY) testrend testrend.bin

readdata: crt0.so readdata.so
	$(SH4CC) $(SH4LDFLAGS) $^ -o $@ $(SH4LIBS)

.PHONY : clean
clean:
	rm -f *.o *.so *.ao *.ac *.bin mapleid ide readmem dumpasic

