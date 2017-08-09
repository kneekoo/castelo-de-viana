CXXFLAGS = -D__i386__ -DEA_PLATFORM_LINUX -DEA_PROCESSOR_X86=1 -DEA_SYSTEM_LITTLE_ENDIAN=1 -g -c -Istb -O6 -Ofast  -funroll-loops -funroll-all-loops -fomit-frame-pointer -fno-exceptions -fno-rtti -ffast-math -mtune=i486 -march=i486 -fpermissive -IEASTL/include -IEASTL/test/packages/EABase/include/Common

CXX = i586-pc-msdosdjgpp-g++

OBJS = Main.o \
    Game.o \
    LoadImage.o \
    NativeBitmap.o \
    Music.o

TARGET = viana.exe

$(TARGET):	$(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) $(LDFLAGS)

all:	$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
	rm *~
