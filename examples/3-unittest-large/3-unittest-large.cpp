// Test for Macronix MX25L25645G (256 Mbit/32 Mbyte) that requires 32-bit addressing mode
#include "Particle.h"

#include "SpiFlashRK.h"

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(MANUAL);

SerialLogHandler logHandler(LOG_LEVEL_TRACE);

// Pick a chip, port, and CS line
// SpiFlashISSI spiFlash(SPI, A2);
// SpiFlashWinbond spiFlash(SPI, A2);
SpiFlashMacronix spiFlash(SPI, A2);
// SpiFlashWinbond spiFlash(SPI1, D5);
//SpiFlashMacronix spiFlash(SPI1, D5);

unsigned long delayTime = 4000;
uint8_t buf1[256];
uint8_t buf2[1024];

class LogTime {
public:
	inline LogTime(const char *desc) : desc(desc), start(millis()) {
		Log.info("starting %s", desc);
	}
	inline ~LogTime() {
		Log.info("finished %s: %lu ms", desc, millis() - start);
	}

	const char *desc;
	unsigned long start;
};

void setup() {
	// Wait for a USB serial connection for up to 10 seconds
  	waitFor(Serial.isConnected, 10000);
  	
	spiFlash.begin();

}

void runTestSuite() {
	//
    Log.info("resetDevice");
    spiFlash.resetDevice();

	Log.info("jedecId=%06lx", spiFlash.jedecIdRead());

	if (!spiFlash.isValid()) {
		Log.error("no valid flash chip");
		return;
	}

    Log.info("enabling 4 byte addressing");
    bool bResult = spiFlash.set4ByteAddressing(true);
    if (!bResult) {
		Log.error("failed to enable 4 byte addressing");
		return;
    }


    Log.info("Note: chipErase will take more than 3 minutes on the 256 Mbit flash chips!");

	{
		LogTime time("chipErase");
		spiFlash.chipErase();
	}

	Log.info("running tests...");

	// Make sure it's erased
	spiFlash.readData(0, buf1, 256);

	for(size_t ii = 0; ii < 256; ii++) {
		if (buf1[ii] != 0xff) {
			Log.error("failure line %d ii=%d value=%02x", __LINE__, ii, buf1[ii]);
			return;
		}
	}

	// Write a whole page
	for(size_t ii = 0; ii < 256; ii++) {
		buf1[ii] = (uint8_t)ii;
	}
	{
		LogTime time("writePage");
		spiFlash.writeData(0, buf1, sizeof(buf1));
	}

	memset(buf1, 0, sizeof(buf1));
	spiFlash.readData(0, buf1, 256);
	for(size_t ii = 0; ii < 256; ii++) {
		if (buf1[ii] != (uint8_t)ii) {
			Log.error("failure line %d ii=%d value=%02x", __LINE__, ii, buf1[ii]);
			return;
		}
	}

	{
		LogTime time("writePage one byte at a time");
		for(size_t ii = 0; ii < 256; ii++) {
			uint8_t temp = (uint8_t)ii;
			spiFlash.writeData(256 + ii, &temp, 1);
		}
	}

	{
		LogTime time("readPage one byte at a time");
		for(size_t ii = 0; ii < 256; ii++) {
			uint8_t temp = 0;
			spiFlash.readData(256 + ii, &temp, 1);
			if (temp != (uint8_t)ii) {
				Log.error("failure line %d ii=%d value=%02x", __LINE__, ii, temp);
				return;
			}
		}
	}
	memset(buf1, 0, sizeof(buf1));
	spiFlash.readData(0, buf1, 256);
	for(size_t ii = 0; ii < 256; ii++) {
		if (buf1[ii] != (uint8_t)ii) {
			Log.error("failure line %d ii=%d value=%02x", __LINE__, ii, buf1[ii]);
			return;
		}
	}

	// Write across page boundaries
	for(size_t ii = 0; ii < 256; ii++) {
		buf1[ii] = (uint8_t)ii;
	}
	{
		LogTime time("write across page boundary");
		spiFlash.writeData(640, buf1, sizeof(buf1));
	}

	{
		for(size_t ii = 0; ii < 256; ii++) {
			uint8_t temp = 0;
			spiFlash.readData(640 + ii, &temp, 1);
			if (temp != (uint8_t)ii) {
				Log.error("failure line %d ii=%d value=%02x", __LINE__, ii, temp);
				return;
			}
		}
	}

	{
		// Read across page boundary
		spiFlash.readData(640, buf1, sizeof(buf1));
		for(size_t ii = 0; ii < 256; ii++) {
			if (buf1[ii] != (uint8_t)ii) {
				Log.error("failure line %d ii=%d value=%02x", __LINE__, ii, buf1[ii]);
				return;
			}
		}
	}


	// Write 1K
	srand(0);
	for(size_t ii = 0; ii < sizeof(buf2); ii++) {
		buf2[ii] = (uint8_t) rand();
	}

	{
		LogTime time("write 1K");
		spiFlash.writeData(1024, buf2, sizeof(buf2));
	}

	memset(buf2, 0, sizeof(buf2));
	{
		LogTime time("read 1K");
		spiFlash.readData(1024, buf2, sizeof(buf2));
	}
	srand(0);
	for(size_t ii = 0; ii < sizeof(buf2); ii++) {
		if (buf2[ii] != (uint8_t)rand()) {
			Log.error("failure line %d ii=%d value=%02x", __LINE__, ii, buf2[ii]);
			return;
		}
	}

	// Write 256K in 1024 chunks of 256 bytes (1 page) starting at 4096
	srand(0);

	{
		LogTime time("write 256K");

		srand(0);

		for(size_t pageCount = 0; pageCount < 1024; pageCount++) {
			for(size_t ii = 0; ii < sizeof(buf1); ii++) {
				buf1[ii] = (uint8_t) rand();
			}

			spiFlash.writeData(4096 + pageCount * 256, buf1, sizeof(buf1));
		}
	}

	{
		LogTime time("read 256K");

		srand(0);

		for(size_t pageCount = 0; pageCount < 1024; pageCount++) {
			memset(buf1, 0, sizeof(buf1));
			spiFlash.readData(4096 + pageCount * 256, buf1, sizeof(buf1));

			for(size_t ii = 0; ii < sizeof(buf1); ii++) {
				if (buf1[ii] != (uint8_t)rand()) {
					Log.error("failure line %d ii=%d value=%02x", __LINE__, ii, buf1[ii]);
					return;
				}
			}

		}
	}

	{
		LogTime time("sectorErase");
		spiFlash.sectorErase(8192);
	}
    // Make sure writes are working above
	{
		LogTime time("verify sector erase");
        srand(0);

        for(size_t pageCount = 0; pageCount < 1024; pageCount++) {
            memset(buf1, 0, sizeof(buf1));
            spiFlash.readData(4096 + pageCount * 256, buf1, sizeof(buf1));

            if (pageCount < 16 || pageCount >= 32) {
                for(size_t ii = 0; ii < sizeof(buf1); ii++) {
                    if (buf1[ii] != (uint8_t)rand()) {
                        Log.error("failure line %d ii=%d value=%02x", __LINE__, ii, buf1[ii]);
                        return;
                    }
                }
            }
            else {
                for(size_t ii = 0; ii < sizeof(buf1); ii++) {
                    rand();
                    if (buf1[ii] != 0xff) {
                        Log.error("failure line %d ii=%d value=%02x", __LINE__, ii, buf1[ii]);
                        return;
                    }
                }
            }

        }
    }


	{
		LogTime time("verify read/write in 32-bit mode");

        srand(0);

        for(size_t ii = 0; ii < 512; ii++) {
			buf2[ii] = (uint8_t) rand();
        }

        size_t boundary = 16777216; // 2^24

        spiFlash.writeData(boundary - 256, buf2, 256);
        spiFlash.writeData(boundary, &buf2[256], 256);
        

        memset(buf1, 0, 256);
        spiFlash.readData(boundary - 256, buf1, 256);
        for(size_t ii = 0; ii < 256; ii++) {
            if (buf1[ii] != buf2[ii]) {
                Log.error("failure line %d ii=%d value=%02x expected=%02x", __LINE__, ii, buf1[ii], buf2[ii]);
                return;
            }
        }

        memset(buf1, 0, 256);
        spiFlash.readData(boundary, buf1, 256);
        for(size_t ii = 0; ii < 256; ii++) {
            if (buf1[ii] != buf2[ii + 256]) {
                Log.error("failure line %d ii=%d value=%02x expected=%02x", __LINE__, ii, buf1[ii], buf2[ii + 256]);
                return;
            }
        }

	}


	Log.info("test complete!");

}


void loop() {
	delay(delayTime);
	delayTime = 60000;

	runTestSuite();
}



