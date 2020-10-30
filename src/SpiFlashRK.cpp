#include "Particle.h"

#include "SpiFlashRK.h"


SpiFlash::SpiFlash(SPIClass &spi, int cs) : spi(spi), cs(cs) {
	}

SpiFlash::~SpiFlash() {

}

void SpiFlash::begin() {
	spi.begin(cs);

	digitalWrite(cs, HIGH);

	// Send release from powerdown 0xab
	wakeFromSleep();
}

bool SpiFlash::isValid() {
	uint8_t foundManufacturerId = (jedecIdRead() >> 16) & 0xff;

	return manufacturerId == foundManufacturerId;
}


void SpiFlash::beginTransaction() {
	
	__SPISettings settings(spiClockSpeedMHz * MHZ, spiBitOrder, spiDataMode);

	spi.beginTransaction(settings);
	pinResetFast(cs);

	// There is some code to do this in the STM32F2xx HAL, but I don't think it's necessary to put
	// a really tiny delay before doing the SPI transfer
	// asm("mov r2, r2");
}

void SpiFlash::endTransaction() {
	pinSetFast(cs);
	spi.endTransaction();
}

uint32_t SpiFlash::jedecIdRead() {

	uint8_t txBuf[4], rxBuf[4];
	txBuf[0] = 0x9f;

	beginTransaction();
	spi.transfer(txBuf, rxBuf, sizeof(txBuf), NULL);
	endTransaction();

	return (rxBuf[1] << 16) | (rxBuf[2] << 8) | (rxBuf[3]);
}

uint8_t SpiFlash::readStatus() {
	uint8_t txBuf[2], rxBuf[2];
	txBuf[0] = 0x05; // RDSR
	txBuf[1] = 0;

	beginTransaction();
	spi.transfer(txBuf, rxBuf, sizeof(txBuf), NULL);
	endTransaction();

	return rxBuf[1];
}

uint8_t SpiFlash::readConfiguration() {
	uint8_t txBuf[2], rxBuf[2];
	txBuf[0] = 0x15; // RDCR
	txBuf[1] = 0;

	beginTransaction();
	spi.transfer(txBuf, rxBuf, sizeof(txBuf), NULL);
	endTransaction();

	return rxBuf[1];
}


bool SpiFlash::isWriteInProgress() {
	return (readStatus() & STATUS_WIP) != 0;
}

void SpiFlash::waitForWriteComplete(unsigned long timeout) {
	unsigned long startTime = millis();

	if (timeout == 0) {
		timeout = waitWriteCompletionTimeoutMs;
	}

	// Wait for up to 500 ms. Most operations should take much less than that.
	while(isWriteInProgress() && millis() - startTime < timeout) {
		// For long timeouts, yield the CPU
		if (timeout > 500) {
			delay(1);
		}
	}

	// Log.trace("isWriteInProgress=%d time=%u", isWriteInProgress(), millis() - startTime);
}


void SpiFlash::writeStatus(uint8_t status) {
	waitForWriteComplete();

	uint8_t txBuf[2];
	txBuf[0] = 0x01; // WRSR
	txBuf[1] = status;

	beginTransaction();
	spi.transfer(txBuf, NULL, sizeof(txBuf), NULL);
	endTransaction();
}

void SpiFlash::readData(size_t addr, void *buf, size_t bufLen) {
	uint8_t *curBuf = (uint8_t *)buf;

	while(bufLen > 0) {
		size_t pageOffset = addr % pageSize;
		size_t pageStart = addr - pageOffset;

		size_t count = (pageStart + pageSize) - addr;
		if (count > bufLen) {
			count = bufLen;
		}

		uint8_t txBuf[5];

		setInstWithAddr(0x03, addr, txBuf); // READ

		beginTransaction();
		spi.transfer(txBuf, NULL, getInstWithAddrSize(), NULL);
		spi.transfer(NULL, curBuf, bufLen, NULL);
		endTransaction();

		addr += count;
		curBuf += count;
		bufLen -= count;
	}
}


void SpiFlash::setInstWithAddr(uint8_t inst, size_t addr, uint8_t *buf) {
	uint8_t *p = buf;
	*p++ = inst;
	if (addr4byte) {
		*p++ = (uint8_t) (addr >> 24);
	}
	*p++ = (uint8_t) (addr >> 16);
	*p++ = (uint8_t) (addr >> 8);
	*p++ = (uint8_t) addr;
}

size_t SpiFlash::getInstWithAddrSize() const {
	return addr4byte ? 5 : 4;	
}


void SpiFlash::writeData(size_t addr, const void *buf, size_t bufLen) {
	uint8_t *curBuf = (uint8_t *)buf;

	waitForWriteComplete();

	while(bufLen > 0) {
		size_t pageOffset = addr % pageSize;
		size_t pageStart = addr - pageOffset;

		size_t count = (pageStart + pageSize) - addr;
		if (count > bufLen) {
			count = bufLen;
		}

		// Log.info("writeData addr=%lx pageOffset=%lu pageStart=%lu count=%lu pageSize=%lu", addr, pageOffset, pageStart, count, pageSize);

		uint8_t txBuf[5];

		setInstWithAddr(0x02, addr, txBuf); // PAGE_PROG

		writeEnable();

		beginTransaction();
		spi.transfer(txBuf, NULL, getInstWithAddrSize(), NULL);
		spi.transfer(curBuf, NULL, count, NULL);
		endTransaction();

		waitForWriteComplete(pageProgramTimeoutMs);

		addr += count;
		curBuf += count;
		bufLen -= count;
	}

}


void SpiFlash::sectorErase(size_t addr) {
	waitForWriteComplete();

	uint8_t txBuf[5];

	// Log.trace("sectorEraseCmd=%02x", sectorEraseCmd);

	//
	// ISSI 25LQ080 uses 0x20 or 0xD7
	// Winbond uses 0x20 only, so use that
	setInstWithAddr(0x20, addr, txBuf); // SECTOR_ER


	writeEnable();

	beginTransaction();
	spi.transfer(txBuf, NULL, getInstWithAddrSize(), NULL);
	endTransaction();

	waitForWriteComplete(sectorEraseTimeoutMs);
}

void SpiFlash::blockErase(size_t addr) {
	waitForWriteComplete();

	uint8_t txBuf[5];

	setInstWithAddr(0xD8, addr, txBuf); // BLOCK_ER

	writeEnable();

	beginTransaction();
	spi.transfer(txBuf, NULL, getInstWithAddrSize(), NULL);
	endTransaction();

	waitForWriteComplete(chipEraseTimeoutMs);

}

void SpiFlash::chipErase() {
	waitForWriteComplete();

	uint8_t txBuf[1];

	txBuf[0] = 0xC7; // CHIP_ER

	writeEnable();

	beginTransaction();
	spi.transfer(txBuf, NULL, sizeof(txBuf), NULL);
	endTransaction();

	waitForWriteComplete(chipEraseTimeoutMs);
}

void SpiFlash::resetDevice() {
	waitForWriteComplete();

	uint8_t txBuf[1];

	txBuf[0] = 0x66; // Enable reset

	beginTransaction();
	spi.transfer(txBuf, NULL, sizeof(txBuf), NULL);
	endTransaction();

	delayMicroseconds(1);

	txBuf[0] = 0x99; // Reset

	beginTransaction();
	spi.transfer(txBuf, NULL, sizeof(txBuf), NULL);
	endTransaction();

	delayMicroseconds(1);
}

void SpiFlash::wakeFromSleep() {
	// Send release from powerdown 0xab
	uint8_t txBuf[1];
	txBuf[0] = 0xab;

	beginTransaction();
	spi.transfer(txBuf, NULL, sizeof(txBuf), NULL);
	endTransaction();

	// Need to wait tres (3 microseconds) before issuing the next command
	delayMicroseconds(3);
}

// Note: not all chips support this. Macronix does.
void SpiFlash::deepPowerDown() {

	uint8_t txBuf[1];
	txBuf[0] = 0xb9;

	beginTransaction();
	spi.transfer(txBuf, NULL, sizeof(txBuf), NULL);
	endTransaction();

	// Need to wait tdp (10 microseconds) before issuing the next command, but since we're probably doing
	// this before sleep, it's not necessary
}


void SpiFlash::writeEnable() {
	uint8_t txBuf[1];

	beginTransaction();
	txBuf[0] = 0x06; // WREN
	spi.transfer(txBuf, NULL, sizeof(txBuf), NULL);
	endTransaction();

	// ISSI devices require a 3us delay here, but Winbond devices do not
	if (writeEnableDelayUs > 0) {
		delayMicroseconds(writeEnableDelayUs);
	}
}

bool SpiFlash::set4ByteAddressing(bool enable) {

	uint8_t txBuf[1];
	txBuf[0] = enable ? 0xb7 : 0xe9; // EN4B / EX4B

	beginTransaction();
	spi.transfer(txBuf, NULL, sizeof(txBuf), NULL);
	endTransaction();

	// Verify that the mode was set
	uint8_t configReg = readConfiguration();
	if ((configReg & 0x20) != 0) { // 4 BYTE
		if (!enable) {
			// Failed to disable
			return false;
		}
	}
	else {
		if (enable) {
			// Failed to enable
			return false;
		}
	}

	addr4byte = enable;
	return true;
}


#if PLATFORM_ID==8

#include "spi_flash.h"

SpiFlashP1::SpiFlashP1() {

}
SpiFlashP1::~SpiFlashP1() {

}

void SpiFlashP1::begin() {
	sFLASH_Init();
}

bool SpiFlashP1::isValid() {
	// TODO: Check the value from jedecIdRead
	return true;
}

uint32_t SpiFlashP1::jedecIdRead() {
	return sFLASH_ReadID();
}

void SpiFlashP1::readData(size_t addr, void *buf, size_t bufLen) {
	sFLASH_ReadBuffer((uint8_t *)buf, addr,  bufLen);
}

void SpiFlashP1::writeData(size_t addr, const void *buf, size_t bufLen) {
	sFLASH_WriteBuffer((const uint8_t *)buf, addr, bufLen);
}

void SpiFlashP1::sectorErase(size_t addr) {
	sFLASH_EraseSector(addr);
}

void SpiFlashP1::chipErase() {
	sFLASH_EraseBulk();
}


#endif

