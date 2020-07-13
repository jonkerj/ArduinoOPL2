// TODO: Rename getters that return booleans to is....
// TODO: Make the get/setSynthMode simpler to understand...

#include "OPL3Duo.h"

// TODO: Change BOARD_TYPE to PLATFORM
#if BOARD_TYPE == OPL2_BOARD_TYPE_ARDUINO
	#include <SPI.h>
	#include <Arduino.h>
#else
	#include <wiringPi.h>
	#include <wiringPiSPI.h>
#endif


OPL3Duo::OPL3Duo() : OPL3() {
}


/**
 * Initialize the OPL3 library and reset the chips.
 */
void OPL3Duo::begin() {
    pinMode(pinUnit, OUTPUT);
	digitalWrite(pinUnit, LOW);

	OPL3::begin();
}


/**
 * Create shadow registers to hold the values written to the OPL3 chips for later access. Don't be greedy on memory,
 * only claim the 478 bytes of memory of valid registers for the two OPL3s to be as Arduino friendly as we can be.
 */
void OPL3Duo::createShadowRegisters() {
	chipRegisters = new byte[5 * 2];					// 	10
	channelRegisters = new byte[3 * numChannels];		// 108
	operatorRegisters = new byte[10 * numChannels];		// 360
}


/**
 * Hard reset the OPL3 chip. All registers will be reset to 0x00, This should be done before sending any register data
 * to the chip.
 */
void OPL3Duo::reset() {
	// Hard reset both OPL3 chips.
    for (byte unit = 0; unit < 2; unit ++) {
        digitalWrite(pinUnit, unit == 1);
        digitalWrite(pinReset, LOW);
        delay(1);
        digitalWrite(pinReset, HIGH);
    }

    // Initialize chip registers on both synth units.
    for (byte i = 0; i < 2; i ++) {
    	setChipRegister(i, 0x01, 0x00);
    	setChipRegister(i, 0x04, 0x00);
    	setChipRegister(i, 0x05, 0x00);
    	setChipRegister(i, 0x08, 0x00);
    	setChipRegister(i, 0xBD, 0x00);
    }

    // Initialize all channel and operator registers.
    for (byte i = 0; i < numChannels; i ++) {
    	setChannelRegister(0xA0, i, 0x00);
    	setChannelRegister(0xB0, i, 0x00);
    	setChannelRegister(0xC0, i, 0x00);

    	for (byte j = OPERATOR1; j <= OPERATOR2; j ++) {
    		setOperatorRegister(0x20, i, j, 0x00);
    		setOperatorRegister(0x40, i, j, 0x00);
    		setOperatorRegister(0x60, i, j, 0x00);
    		setOperatorRegister(0x80, i, j, 0x00);
    		setOperatorRegister(0xE0, i, j, 0x00);
    	}
    }

    digitalWrite(pinUnit, LOW);
}


/**
 * Get the current value of a chip wide register from the shadow registers.
 * 
 * @param synthUnit - The chip to address [0, 1]
 * @param reg - The 9-bit address of the register.
 * @return The value of the register from shadow registers.
 */
byte OPL3Duo::getChipRegister(byte synthUnit, short reg) {
	synthUnit = synthUnit & 0x01;
	return chipRegisters[(synthUnit * 5) + getChipRegisterOffset(reg)];
}


/**
 * Write a given value to a chip wide register.
 *
 * @param synthUnit - The chip to address [0, 1]
 * @param reg - The 9-bit register to write to.
 * @param value - The value to write to the register.
 */
void OPL3Duo::setChipRegister(byte synthUnit, short reg, byte value) {
	synthUnit = synthUnit & 0x01;
	chipRegisters[(synthUnit * 5) + getChipRegisterOffset(reg)] = value;

	byte bank = ((reg >> 8) & 0x01) | (synthUnit << 1);
	write(bank, reg & 0xFF, value);
}


/**
 * Write a given value to a channel based register.
 *
 * @param baseRegister - The base address of the register.
 * @param channel - The channel to address [0, 17]
 * @param value - The value to write to the register.
 */
void OPL3Duo::setChannelRegister(byte baseRegister, byte channel, byte value) {
	channelRegisters[getChannelRegisterOffset(baseRegister, channel)] = value;

	byte bank = (channel / CHANNELS_PER_BANK) & 0x03;
	byte reg = baseRegister + (channel % CHANNELS_PER_BANK);
	write(bank, reg, value);
}


/**
 * Write a given value to an operator register for a channel.
 *
 * @param baseRegister - The base address of the register.
 * @param channel - The channel of the operator [0, 17]
 * @param op - The operator to change [0, 1].
 * @param value - The value to write to the operator's register.
 */
void OPL3Duo::setOperatorRegister(byte baseRegister, byte channel, byte operatorNum, byte value) {
	operatorRegisters[getOperatorRegisterOffset(baseRegister, channel, operatorNum)] = value;

	byte bank = (channel / CHANNELS_PER_BANK) & 0x03;
	byte reg = baseRegister + getRegisterOffset(channel % CHANNELS_PER_BANK, operatorNum);
	write(bank, reg, value);
}


/**
 * Write a given value to a register of the OPL3 chip.
 *
 * @param bank - The bank + unit (A1 + A2) of the register [0, 3].
 * @param reg - The register to be changed.
 * @param value - The value to write to the register.
 */
void OPL3Duo::write(byte bank, byte reg, byte value) {
    digitalWrite(pinUnit, bank & 0x02);
    OPL3::write(bank, reg, value);
}


/**
 * Returns whether OPL3 mode is currently enabled on both synth units.
 *
 * @return True if OPL3 mode is enabled.
 */
bool OPL3Duo::isOPL3Enabled(byte synthUnit) {
	return getChipRegister(synthUnit & 0x01, 0x105) & 0x01;
}


/**
 * Returns whether OPL3 mode is currently enabled on both synth units.
 *
 * @return True if OPL3 mode is enabled on both chips.
 */
bool OPL3Duo::isOPL3Enabled() {
	return (getChipRegister(0, 0x105) & 0x01) == 0x01 &&
		(getChipRegister(1, 0x105) & 0x01) == 0x01;
}


/**
 * Enable or disable OPL3 mode on both synth units. This function must be called in order to use any of the OPL3
 * functions. It will also set panning for all channels to enable both left and right speakers when OPL3 mode is
 * enabled.
 *
 * @param enable - When set to true enables OPL3 mode.
 */
void OPL3Duo::setOPL3Enabled(bool enable) {
	setChipRegister(0, 0x105, enable ? 0x01 : 0x00);
	setChipRegister(1, 0x105, enable ? 0x01 : 0x00);

	// For ease of use enable both the left and the right speaker on all channels when going into OPL3 mode.
	for (byte i = 0; i < numChannels; i ++) {
		setPanning(i, enable, enable);
	}
}


/**
 * Enable or disable OPL3 mode for a single synth unit only. This function must be called in order to use any of the
 * OPL3 functions. It will also set panning for all channels to enable both left and right speakers when OPL3 mode is
 * enabled.
 *
 * @param synthUnit - Synth unit [0, 1] for which to change OPL3 mode.
 * @param enable - When set to true enables OPL3 mode.
 */
void OPL3Duo::setOPL3Enabled(byte synthUnit, bool enable) {
	synthUnit = synthUnit & 0x01;
	setChipRegister(synthUnit, 0x105, enable ? 0x01 : 0x00);

	// For ease of use enable both the left and the right speaker on all channels when going into OPL3 mode.
	byte firstChannel = synthUnit == 0 ? 0 : numChannels / 2;
	byte lastChannel  = synthUnit == 0 ? numChannels / 2 : numChannels;
	for (byte i = firstChannel; i < lastChannel; i ++) {
		setPanning(i, enable, enable);
	}
}
