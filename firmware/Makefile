CFLAGS=-g -mcpu=cortex-m7 -DSTM32H735 -O0 -DMICROKVS_WRITE_BLOCK_SIZE=32
CXXFLAGS=$(CFLAGS) --std=c++17 -fno-exceptions -fno-rtti -g --specs=nano.specs \
	-I../stm32-cpp/devices/inc/ \
	-I../stm32-cpp/src/ \
	-I../ \
	-Iconfig
CC=arm-none-eabi-gcc
CXX=arm-none-eabi-g++
all:
	$(CXX) *.cpp -c $(CXXFLAGS)
	$(CC) ../stm32-cpp/src/cpu/*.S -c $(CFLAGS)
	$(CXX) ../embedded-cli/*.cpp -c $(CXXFLAGS)
	$(CXX) ../stm32-cpp/src/cli/*.cpp -c $(CXXFLAGS)
	$(CXX) ../stm32-cpp/src/newlib-stubs/*.cpp -c $(CXXFLAGS)
	$(CXX) ../stm32-cpp/src/peripheral/*.cpp -c $(CXXFLAGS)
	$(CXX) ../stm32-cpp/src/util/*.cpp -c $(CXXFLAGS)
	$(CXX) ../stm32-cpp/devices/src/stm32h735.cpp -c $(CXXFLAGS)
	$(CXX) $(CXXFLAGS) *.o -Wl,-T ../stm32-cpp/devices/link/stm32h735.ld -o firmware.elf
	arm-none-eabi-objcopy -O binary --only-section=.text --only-section=.data firmware.elf firmware.bin
	./imagesize.sh
