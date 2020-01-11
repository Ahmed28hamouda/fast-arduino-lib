#include <fastarduino/gpio.h>
#include <fastarduino/realtime_timer.h>

REGISTER_RTT_ISR(0)

const constexpr uint32_t BLINK_DELAY_MS = 500;

int main()
{
	board::init();
	sei();

	timer::RTT<board::Timer::TIMER0> rtt;
	rtt.begin();

	gpio::FAST_PIN<board::DigitalPin::LED> led{gpio::PinMode::OUTPUT};
	while (true)
	{
		led.toggle();
		rtt.delay(BLINK_DELAY_MS);
	}
}
