/*
 * CommsController.cpp
 *
 * Created: 02-Sept-17 13:02:26
 * ELECTENG 311 Smart Fan Project
 * Group 4
 */
#include "CommsController.h"
#include <avr/eeprom.h>

// Retrieved from http://www.edaboard.com/entry862.html
#define read_eeprom_array(address,value_p,length) eeprom_read_block ((void *)value_p, (const void *)address, length)

CommsController::CommsController(uint8_t ubrr) {
	UCSR0B |= (1<<RXEN0)|(1<<TXEN0)|(1<<RXCIE0); // Enable Rx, Tx and Rx Complete Interrupt.
	UBRR0H = (ubrr>>8);
	UBRR0L = ubrr;
	// Select 8-bit data frame, single stop bit and no parity using UCSR0C (Default values are what we want).

	this->bjsonComplete = false;
	this->bReTransmit = false;
	this->terminatingChar = 0;

	// Create a new tinyJSONpp Object. (accepts max json size of 150 due to current max json object size of 127 bytes).
	json = new tinyjsonpp(false, 150);
}

void CommsController::transmit(uint8_t data) volatile {
	UCSR0B &= ~(1<<RXCIE0); // Disable Rx interrupt.
	while (!(UCSR0A & (1<<UDRE0))); // Wait for empty transmit buffer.

	UDR0 = data; // Put data in UDR for transmission.

	while (!(UCSR0A & (1<<TXC0)));
	UCSR0B |= (1<<RXCIE0); // Re-enable Rx interrupt.
}

void CommsController::run() volatile {

	// Checking if Rx is complete.
	if (this->bjsonComplete || this->bReTransmit) {
		UCSR0B &= ~(1<<RXCIE0); // Disable Rx interrupt.

		if ((json->getChar(2) == '3' && (json->getChar(3) == '"')) || this->bReTransmit) { // Checking that Rx is meant to be for our fan.
			Value val;

			// Allocate heap space for reading EEPROM information into.
			char* key = static_cast<char *>(calloc(4, sizeof(char))); // Allocating 4 bytes for the key.
			char* value = static_cast<char *>(calloc(42, sizeof(char))); // Allocating 50 bytes for the value. (Since max is 42 bytes).
			char* parent = static_cast<char *>(calloc(4, sizeof(char))); // Allocating 4 bytes for the parent.

			// Set current key to req.
			read_eeprom_array(19, key, 4);
			val = json->getValue(key);
			if (val.size > 0) {
				speedController->setFanSpeed(json->convertValueToInt(val));

				// Ignore lockedRotor for initially changing speed.
				errorHandler->lockedRotor = false;
			}

			// Set current key to clr.
			read_eeprom_array(31, key, 4);
			val = json->getValue(key);
			if (val.size > 0){
				// Clear errors.
				errorHandler->clearErrors();
			}

			// Reconstruct our json message.
			json->reset();

			// ------------- ROOT JSON SETUP ---------------
			// Set key to root.
			read_eeprom_array(0, key, 2);
			// Set value to obj.
			read_eeprom_array(2, value, 3);
			json->insert(key, value, '\0');

			// ------------- SOFTWARE VERSION --------------
			// Set key to ver.
			read_eeprom_array(5, key, 4);
			// Set value to softVer.
			read_eeprom_array(9, value, 6);
			// Set parent to root.
			read_eeprom_array(0, parent, 2);
			json->insert(key, value, parent);

			// ------------------ POWER --------------------
			// Set key to pwr.
			read_eeprom_array(27, key, 4);
			// Converting requested power value from float to string.
			volatile uint8_t power = (uint8_t)((0.008788f * speedController->currentSpeed + 0.07952f) * 10.0f); // Times Power float by 10 to get the x.xyy to xx.yy, cast to int to remove decimal.
			itoa(power, value, 10); // Converting requested power value from int to string.
			if (power > 9) {
				value[2] = value[1];
				value[1] = '.';
			} else {
				value[2] = value[0];
				if (power == 0) {
					value[2] = '0';
				}
				value[0] = '0';
				value[1] = '.';
			}
			value[3] = 'W';
			value[4] = '\0';
			
			// Set parent to root.
			read_eeprom_array(0, parent, 2);
			// Inserting power values.
			json->insert(key, value, parent);

			// ------------------ ERRORS -------------------
			bool errors = errorHandler->checkForErrors();
			if (errors) {
				// Set key to ew.
				read_eeprom_array(35, key, 3);
				// TODO: SET VALUE TO THE CORRECT ERROR.
				// Set value to error based on stored error EEPROM locations.
				read_eeprom_array(errorHandler->errorEEPROMStart, value, errorHandler->errorEEPROMLength);
				// Set parent to root.
				read_eeprom_array(0, parent, 2);
				// Inserting errors.
				json->insert(key, value, parent);
			}

			// ------------------ SPEED --------------------
			// Set key to spd.
			read_eeprom_array(15, key, 4);
			// Set value to obj.
			read_eeprom_array(2, value, 3);
			// Set parent to root.
			read_eeprom_array(0, parent, 2);
			// Setup spd object.
			json->insert(key, value, parent);

			// Set key to req.
			read_eeprom_array(19, key, 4);
			// Set req speed value.
			itoa(speedController->requestedSpeed, value, 10); // Converting requested speed value from int to string.
			// Set parent to spd.
			read_eeprom_array(15, parent, 4);
			json->insert(key, value, parent);

			// Set key to cur.
			read_eeprom_array(23, key, 4);
			itoa(speedController->currentSpeed, value, 10); // Converting current speed value from int to string.
			// Parent already correct.
			json->insert(key, value, parent);

			free(key);
			free(value);
			free(parent);

			uint8_t size = json->getJSONSize();
			// For loop to transmit every bit of json.
			for (uint8_t i = 0; i < size; ++i){
				this->transmit(json->getChar(i));
			}

			// Re-transmit the terminating character sent to us.
			this->transmit(this->terminatingChar);
			this->terminatingChar = 0;
		}

		json->empty(); // Reset the JSON string so the Heap does not overflow!
		this->bjsonComplete = false; // Resetting json complete.
		this->bReTransmit = false;

		UCSR0B |= (1<<RXCIE0); // Re-enable Rx interrupt.
	}
}

void CommsController::setControllerPointers(volatile SpeedController* speedController, volatile PowerController* powerController, volatile ErrorHandler* errorHandler) volatile {
	this->speedController = speedController;
	this->powerController = powerController;
	this->errorHandler = errorHandler;
}
