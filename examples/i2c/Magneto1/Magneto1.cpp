//   Copyright 2016-2020 Jean-Francois Poilpret
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.

/*
 * Check HMC5883L I2C device (3D compass) and display output to the UART console.
 * This program uses FastArduino HMC5883L support API.
 * 
 * Wiring:
 * NB: you should add pullup resistors (10K-22K typically) on both SDA and SCL lines.
 * - on ATmega328P based boards (including Arduino UNO):
 *   - A4 (PC4, SDA): connected to HMC5883L SDA pin
 *   - A5 (PC5, SCL): connected to HMC5883L SCL pin
 *   - direct USB access
 * - on Arduino LEONARDO:
 *   - D2 (PD1, SDA): connected to HMC5883L SDA pin
 *   - D3 (PD0, SCL): connected to HMC5883L SDA pin
 *   - direct USB access
 * - on Arduino MEGA:
 *   - D20 (PD1, SDA): connected to HMC5883L SDA pin
 *   - D21 (PD0, SCL): connected to HMC5883L SDA pin
 *   - direct USB access
 * - on ATtinyX4 based boards:
 *   - D6 (PA6, SDA): connected to HMC5883L SDA pin
 *   - D4 (PA4, SCL): connected to HMC5883L SDA pin
 *   - D8 (PB0, TX): connected to SerialUSB converter
 */

#include <fastarduino/time.h>
#include <fastarduino/i2c_manager.h>
#include <fastarduino/devices/hmc5883l.h>

#if defined(ARDUINO_UNO) || defined(ARDUINO_NANO) || defined(BREADBOARD_ATMEGA328P) || defined(ARDUINO_MEGA)
#define HARDWARE_UART 1
#include <fastarduino/uart.h>
static constexpr const board::USART UART = board::USART::USART0;
static constexpr const uint8_t OUTPUT_BUFFER_SIZE = 64;
// Define vectors we need in the example
REGISTER_UATX_ISR(0)
#elif defined(ARDUINO_LEONARDO)
#define HARDWARE_UART 1
#include <fastarduino/uart.h>
static constexpr const board::USART UART = board::USART::USART1;
static constexpr const uint8_t OUTPUT_BUFFER_SIZE = 64;
// Define vectors we need in the example
REGISTER_UATX_ISR(1)
#elif defined(BREADBOARD_ATTINYX4)
#define HARDWARE_UART 0
#include <fastarduino/soft_uart.h>
static constexpr const board::DigitalPin TX = board::DigitalPin::D8_PB0;
static constexpr const uint8_t OUTPUT_BUFFER_SIZE = 64;
#else
#error "Current target is not yet supported!"
#endif

// UART for traces
static char output_buffer[OUTPUT_BUFFER_SIZE];
#if HARDWARE_UART
static serial::hard::UATX<UART> uart{output_buffer};
#else
static serial::soft::UATX<TX> uart{output_buffer};
#endif
static streams::ostream out = uart.out();

using devices::magneto::DataOutput;
using devices::magneto::Gain;
using devices::magneto::HMC5883L;
using devices::magneto::Sensor3D;
using devices::magneto::MeasurementMode;
using devices::magneto::OperatingMode;
using devices::magneto::SamplesAveraged;
using devices::magneto::Status;
using devices::magneto::magnetic_heading;

using streams::dec;
using streams::hex;
using streams::flush;

void trace_status(Status status)
{
	out	<< dec << F("status reserved = ") << status.reserved 
		<< F(", lock = ") << status.lock 
		<< F(", ready = ") << status.ready << '\n' << flush;
}

void trace_fields(const Sensor3D& fields)
{
	out << dec << F("Fields x = ") << fields.x << F(", y = ") << fields.y << F(", z = ") << fields.z << '\n' << flush;
}

using MAGNETOMETER = HMC5883L<i2c::I2CMode::FAST>;

int main() __attribute__((OS_main));
int main()
{
	board::init();
	sei();
	uart.begin(115200);
	out.width(2);
	out << F("Start\n") << flush;
	
	MAGNETOMETER::MANAGER manager;
	manager.begin();
	out << F("I2C interface started\n") << flush;
	out << hex << F("status #1 ") << manager.status() << '\n' << flush;
	
	MAGNETOMETER compass{manager};
	
	bool ok = compass.begin(
		OperatingMode::CONTINUOUS, Gain::GAIN_1_9GA, DataOutput::RATE_75HZ, SamplesAveraged::EIGHT_SAMPLES);
	out << dec << F("begin() ") << ok << '\n' << flush;
	out << hex << F("status #2 ") << manager.status() << '\n' << flush;
	trace_status(compass.status());
	while (true)
	{
		while (!compass.status().ready) ;
		trace_status(compass.status());
		Sensor3D fields;
		ok = compass.magnetic_fields(fields);

		float heading = magnetic_heading(fields.x, fields.y);
		out << F("Magnetic heading ") << heading << F(" rad\n") << flush;
		compass.convert_fields_to_mGA(fields);
		trace_fields(fields);
		time::delay_ms(500);
	}
	
	// Stop TWI interface
	//===================
	manager.end();
	out << hex << F("status #4 ") << manager.status() << '\n' << flush;
	out << F("End\n") << flush;
}
