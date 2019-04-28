#include <fastarduino/time.h>
#include <fastarduino/i2c_manager.h>
#include <fastarduino/devices/ds1307.h>
#include <fastarduino/uart.h>

static constexpr const board::USART UART = board::USART::USART0;
static constexpr const uint8_t OUTPUT_BUFFER_SIZE = 64;
static char output_buffer[OUTPUT_BUFFER_SIZE];

REGISTER_UATX_ISR(0)

using devices::rtc::DS1307;
using devices::rtc::tm;
using namespace streams;

int main() __attribute__((OS_main));
int main()
{
	board::init();
	sei();
	serial::hard::UATX<UART> uart{output_buffer};
	uart.register_handler();
	uart.begin(115200);
	ostream out = uart.out();
	
	i2c::I2CManager<> manager;
	manager.begin();
	DS1307 rtc{manager};
	
	tm now;
	rtc.get_datetime(now);
	out	<< dec << F("RTC: [") 
		<< uint8_t(now.tm_wday) << ']'
		<< now.tm_mday << '.'
		<< now.tm_mon << '.'
		<< now.tm_year << ' '
		<< now.tm_hour << ':'
		<< now.tm_min << ':'
		<< now.tm_sec << endl;
	
	manager.end();
}