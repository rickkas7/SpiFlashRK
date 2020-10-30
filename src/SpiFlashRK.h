/**
 * Library for SPI NOR flash chips for the Particle platform
 *
 * https://github.com/rickkas7/SpiFlashRK
 *
 * License: MIT
 */

#ifndef __SPIFLASHRK_H
#define __SPIFLASHRK_H

#include "Particle.h"

/**
 * @brief Pure virtual base class SPI for SpiFlash devices
 *
 * This is used because the P1 flash uses the system firmware flash API instead of the low-level
 * API in the SpiFlash class, so this provides a common API.
 */
class SpiFlashBase {
public:
	SpiFlashBase() {};
	virtual ~SpiFlashBase() {};

	/**
	 * @brief Call begin, probably from setup(). The initializes the SPI object.
	 */
	virtual void begin() = 0;

	/**
	 * @brief Returns true if there is a flash chip present and it appears to be the correct manufacturer code.
	 */
	virtual bool isValid() = 0;

	/**
	 * @brief Gets the JEDEC ID for the flash device.
	 *
	 * @return A 32-bit value containing the manufacturer ID and the two device IDs:
	 *
	 * byte[0] manufacturer ID mask 0x00ff0000
	 * byte[1] device ID 1     mask 0x0000ff00
	 * byte[2] device ID 2     mask 0x000000ff
	 */
	virtual uint32_t jedecIdRead() = 0;

	/**
	 * @brief Reads data synchronously. Reads data correctly across page boundaries.
	 *
	 * @param addr The address to read from
	 * @param buf The buffer to store data in
	 * @param bufLen The number of bytes to read
	 */
	virtual void readData(size_t addr, void *buf, size_t bufLen) = 0;

	/**
	 * @brief Writes data synchronously. Can write data across page boundaries.
	 *
	 * @param addr The address to read from
	 * @param buf The buffer to store data in
	 * @param bufLen The number of bytes to write
	 */
	virtual void writeData(size_t addr, const void *buf, size_t bufLen) = 0;

	/**
	 * @brief Erases a sector. Sectors are sectorSize bytes and the smallest unit that can be erased.
	 *
	 * This call blocks for the duration of the erase, which take take some time (up to 500 milliseconds).
	 *
	 * @param addr Address of the beginning of the sector. Must be at the start of a sector boundary.
	 */
	virtual void sectorErase(size_t addr) = 0;

	/**
	 * @brief Erases the entire chip.
	 *
	 * This call blocks for the duration of the erase, which take take some time (several secoonds). This function
	 * uses delay(1) so the cloud connection will be serviced in non-system-threaded mode.
	 */
	virtual void chipErase() = 0;

	/**
	 * @brief Gets the page size (default: 256)
	 */
	inline size_t getPageSize() const { return pageSize; };

	/**
	 * @brief Sets the page size (default: 256)
	 */
	inline SpiFlashBase &withPageSize(size_t value) { pageSize = value; return *this; };

	/**
	 * @brief Gets the sector size (default: 4096)
	 */
	inline size_t getSectorSize() const { return sectorSize; };

	/**
	 * @brief Sets the sector size (default: 4096)
	 */
	inline SpiFlashBase &withSectorSize(size_t value) { sectorSize = value; return *this; };

protected:
	size_t pageSize = 256;
	size_t sectorSize = 4096;

};

/**
 * @brief Object for interfacing with an SPI flash chip
 *
 * Normally you would use one of the subclasses like SpiFlashISSI, SpiFlashWinbond, or SpiFlashMacronix
 * and allocate it as a global variable.
 */
class SpiFlash : public SpiFlashBase {
public:
	SpiFlash(SPIClass &spi, int cs);
	virtual ~SpiFlash();

	/**
	 * @brief Call begin, probably from setup(). The initializes the SPI object.
	 */
	void begin();

	/**
	 * @brief Returns true if there is a flash chip present and it appears to be the correct manufacturer code.
	 */
	bool isValid();

	/**
	 * @brief Gets the JEDEC ID for the flash device.
	 *
	 * @return A 32-bit value containing the manufacturer ID and the two device IDs:
	 *
	 * byte[0] manufacturer ID mask 0x00ff0000
	 * byte[1] device ID 1     mask 0x0000ff00
	 * byte[2] device ID 2     mask 0x000000ff
	 */
	uint32_t jedecIdRead();

	/**
	 * @brief Reads the status register
	 */
	uint8_t readStatus();

	/**
	 * @brief Reads the configuration register (RDCR)
	 */
	uint8_t readConfiguration();

	/**
	 * @brief Checks the status register and returns true if a write is in progress
	 */
	bool isWriteInProgress();

	/**
	 * @brief Waits for any pending write operations to complete
	 *
	 * Waits up to waitWriteCompletionTimeoutMs milliseconds (default: 500) if
	 * not specified or 0. Otherwise, waits the specified number of milliseconds.
	 */
	void waitForWriteComplete(unsigned long timeout = 0);

	/**
	 * @brief Writes the status register.
	 */
	void writeStatus(uint8_t status);

	/**
	 * @brief Reads data synchronously. Reads data correctly across page boundaries.
	 *
	 * @param addr The address to read from
	 * @param buf The buffer to store data in
	 * @param bufLen The number of bytes to read
	 */
	void readData(size_t addr, void *buf, size_t bufLen);

	/**
	 * @brief Writes data synchronously. Can write data across page boundaries.
	 *
	 * @param addr The address to read from
	 * @param buf The buffer to store data in
	 * @param bufLen The number of bytes to write
	 */
	void writeData(size_t addr, const void *buf, size_t bufLen);

	/**
	 * @brief Erases a sector. Sectors are 4K (4096 bytes) and the smallest unit that can be erased.
	 *
	 * This call blocks for the duration of the erase, which take take some time (up to 500 milliseconds).
	 *
	 * @param addr Address of the beginning of the sector. Must be at the start of a sector boundary.
	 */
	void sectorErase(size_t addr);

	/**
	 * @brief Erases a block. Blocks are 64K (65536 bytes) or 16 sectors. There are 16 blocks on a 1 MByte device.
	 *
	 * This call blocks for the duration of the erase, which take take some time (up to 1 second).
	 * This call is not in the base API because the P1 does not support it. SPIFFS doesn't need it for operation
	 * and uses sector erase.
	 *
	 * @param addr Address of the beginning of the block
	 */
	void blockErase(size_t addr);

	/**
	 * @brief Erases the entire chip.
	 *
	 * This call blocks for the duration of the erase, which take take some time (several secoonds). This function
	 * uses delay(1) so the cloud connection will be serviced in non-system-threaded mode.
	 */
	void chipErase();

	/**
	 * @brief Sends the device reset sequence
	 *
	 * Winbond devices support this, ISSI devices do not.
	 */
	void resetDevice();

	/**
	 * @brief Wakes the chip from sleep. Not normally necessary, except when doing deep power down on Macronix chips
	 */
	void wakeFromSleep();

	/**
	 * @brief Deep power down. Only supported by Macronix.
	 */
	void deepPowerDown();


	/**
	 * @brief Enable or disable 4-byte addressing mode for devices larger than 128 Mbit
	 * 
	 * @param enable Enable 4-byte addressing if true
	 * 
	 * The default power-on state is 3-byte addressing. 3-byte addressing is used if disabled,
	 * power-on, or device reset.
	 */
	bool set4ByteAddressing(bool enable);

	/**
	 * @brief Sets the page size (default: 256)
	 */
	inline SpiFlash &withPageSize(size_t value) { pageSize = value; return *this; };

	/**
	 * @brief Sets the sector size (default: 4096)
	 */
	inline SpiFlash &withSectorSize(size_t value) { sectorSize = value; return *this; };

	/**
	 * @brief Sets the SPI clock speed (default: 30 MHz)
	 */
	inline SpiFlash &withSpiClockSpeedMHz(uint8_t value) { spiClockSpeedMHz = value; return *this; };

	/**
	 * @brief Sets shared bus mode
	 *
	 * @param delayus Amount of time in microseconds to delay after changing SPI settings to allow the bus to settle.
	 * 
	 * This is no longer necessary and is only included for backward compatibility and does nothing.
	 */
	inline SpiFlash &withSharedBus(unsigned long delayus) { return *this;};

protected:
	// Flags for the status register
	static const uint8_t STATUS_WIP 	= 0x01;
	static const uint8_t STATUS_WEL 	= 0x02;
	static const uint8_t STATUS_SRWD 	= 0x80;

	/**
	 * @brief The expected manufacturerId, used by isValid(). This is overridden by flash-chip-specific subclasses.
	 */
	uint8_t manufacturerId = 0x9d;

	/**
	 * @brief The bit order for SPI. Must be MSBFIRST for SPI flash modules.
	 */
	uint8_t spiBitOrder = MSBFIRST;

	/**
	 * @brief The maximum speed to use. Most flash modules can handle 60 MHz without difficulties.
	 *
	 * Flash-chip-specific subclasses can override this if they need a slower speed.
	 */
	uint8_t spiClockSpeedMHz = 30;

	/**
	 * @brief The data mode.
	 *
	 * Most chips can use SPI_MODE0 or SPI_MODE3 but I've found that Winbond chips don't work very reliably in mode 0
	 * so the default is SPI_MODE3, since both ISSI and Winbond chips work well with MODE3.
	 */
	uint8_t spiDataMode = SPI_MODE3;

	/**
	 * @brief Maximum time to wait for a write call to complete in milliseconds.
	 */
	unsigned long waitWriteCompletionTimeoutMs = 10;

	/**
	 * @brief Maximum amount of time to wait for a sector erase call to complete in milliseconds.
	 */
	unsigned long sectorEraseTimeoutMs = 500;

	/**
	 * @brief Maximum amount of time to wait for a page program call to complete in milliseconds.
	 */
	unsigned long pageProgramTimeoutMs = 10;

	/**
	 * @brief Maximum amount of time to wait for a chip erase call to complete in milliseconds.
	 */
	unsigned long chipEraseTimeoutMs = 50000;

	/**
	 * @brief Amount of time to delay after write enable in microseconds.
	 *
	 * ISSI devices need a delay of Tres, 3 microseconds. Winbond devices do not need a delay.
	 */
	unsigned long writeEnableDelayUs = 3;

private:
	/**
	 * @brief Enables writes to the status register, flash writes, and erases.
	 *
	 * This is used internally before the functions that require it.
	 */
	void writeEnable();

	/**
	 * @brief Begins an SPI transaction, setting the CS line LOW.
	 * Also sets the SPI speed and mode settings if sharedBus == true
	 */
	void beginTransaction();

	/**
	 * @brief Ends an SPI transaction, basically just setting the CS line high.
	 */
	void endTransaction();

	/**
	 * @brief Sets a instruction code and an address 
	 * 
	 * 3-byte, 24-bit, big endian value, except wehn in 4 byte mode, when
	 * it's 4-byte, 32-bit, big endian.
	 */
	void setInstWithAddr(uint8_t inst, size_t addr, uint8_t *buf);

	/**
	 * @brief Gets the size of setInstWithAddr, either 4 or 5 bytes
	 */
	size_t getInstWithAddrSize() const;

	SPIClass &spi;
	int cs;
	bool addr4byte = false;
};

/**
 * @brief Class for ISSI IS245LQ080 SPI NOR flash modules (1 Mbyte)
 */
class SpiFlashISSI : public SpiFlash {
public:
	inline SpiFlashISSI(SPIClass &spi, int cs) : SpiFlash(spi, cs) {
		sectorEraseTimeoutMs = 300;
		pageProgramTimeoutMs = 10; // 1 ms actually
		chipEraseTimeoutMs = 6000;
		manufacturerId = 0x9d;
		writeEnableDelayUs = 3;
	}

};

/**
 * @brief Class for Winbond W25Qxx modules of various sizes
 */
class SpiFlashWinbond : public SpiFlash {
public:
	inline SpiFlashWinbond(SPIClass &spi, int cs) : SpiFlash(spi, cs) {
		sectorEraseTimeoutMs = 500;
		pageProgramTimeoutMs = 10; // 3 ms actually
		chipEraseTimeoutMs = 50000;
		manufacturerId = 0xef;
		writeEnableDelayUs = 0;
	}

};

/**
 * @brief Class for Macronix MX25L8006E, the suggested flash for the Particle E Series module
 * 
 * Timeout of 220 seconds is for the MX25L25645G (110-210 seconds). Note that chip erase
 * only takes as long as is necessary; the timeout is only there in case the device
 * never clears the WIP (write-in-progress) flag, which would be an odd error condition.
 */
class SpiFlashMacronix : public SpiFlash {
public:
	inline SpiFlashMacronix(SPIClass &spi, int cs) : SpiFlash(spi, cs) {
		sectorEraseTimeoutMs = 200;
		pageProgramTimeoutMs = 10; // 1 ms actually
 		chipEraseTimeoutMs = 220000;
		manufacturerId = 0xc2;
		writeEnableDelayUs = 0;
	}

};

// P1 platform only
#if PLATFORM_ID==8

/**
 * @brief Wrapper for the 1 MB flash on the Particle P1 module
 *
 * This SPI flash chip is separate from the one on the STM32F205 module and isn't used by the system firmware
 * so the whole thing can be used for user firmware.
 */
class SpiFlashP1 : public SpiFlashBase {
public:
	SpiFlashP1();
	virtual ~SpiFlashP1();

	virtual void begin();
	virtual bool isValid();
	virtual uint32_t jedecIdRead();
	virtual void readData(size_t addr, void *buf, size_t bufLen);
	virtual void writeData(size_t addr, const void *buf, size_t bufLen);
	virtual void sectorErase(size_t addr);
	virtual void chipErase();
};
#endif

#endif /* __SPIFLASHRK_H */



