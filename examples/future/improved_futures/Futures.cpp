/*
 * This program demonstrates the use Futures listeners and the new FuturesGroup API.
 * 
 * It just uses an Arduino UNO with USB console.
 */

#include <fastarduino/future.h>

#include <fastarduino/uart.h>
#include <fastarduino/iomanip.h>
#include <fastarduino/flash.h>

// Register vector for UART (used for debug)
REGISTER_UATX_ISR(0)

// Comment to use FakeFuture instead of real Future
#define REAL_FUTURE

// Example starts here
//=====================

using namespace future;
using namespace streams;
using namespace containers;

static constexpr const uint8_t OUTPUT_BUFFER_SIZE = 128;
static char output_buffer[OUTPUT_BUFFER_SIZE];

#ifdef REAL_FUTURE
#define ABSTRACTFUTURE AbstractFuture
#define FUTURE Future
#else
#define ABSTRACTFUTURE AbstractFakeFuture
#define FUTURE FakeFuture
#endif

struct FutureListener : FutureStatusListener<ABSTRACTFUTURE>, FutureOutputListener<ABSTRACTFUTURE>
{
	explicit FutureListener(ostream& out) : out_{out} {}
	
	void on_status_change(UNUSED const ABSTRACTFUTURE& future, FutureStatus new_status) override
	{
		out_ << F("on_status_change() status = ") << new_status << endl;
	}
	void on_output_change(UNUSED const ABSTRACTFUTURE& future, uint8_t* output_data, uint8_t* output_current) override
	{
		out_	<< F("on_output_change() data = ") << hex << output_data << F(", current = ") 
				<< hex << output_current << endl;
	}

	ostream& out_;
};

class MyFuture : public FUTURE<uint32_t, uint8_t>
{
	using PARENT = FUTURE<uint32_t, uint8_t>;
	static constexpr uint8_t REG_INDEX = 0x34;

public:
	MyFuture(FutureListener* listener = nullptr) : PARENT{REG_INDEX, listener, listener} {}
	MyFuture(MyFuture&&) = default;
	MyFuture& operator=(MyFuture&&) = default;
};

struct UpdateRegister
{
	UpdateRegister(uint8_t reg_index)
	{
		data[0] = data[1] = reg_index;
	}

	uint8_t data[3] = {0 ,0, 0};
};
class UpdateRegisterFuture : public FUTURE<uint8_t, UpdateRegister>, public FutureOutputListener<ABSTRACTFUTURE>
{
	using PARENT = FUTURE<uint8_t, UpdateRegister>;

public:
	UpdateRegisterFuture(uint8_t reg_index, uint8_t set_mask)
		:	PARENT{UpdateRegister{reg_index}, nullptr, this}, set_mask_{set_mask} {}
	UpdateRegisterFuture(UpdateRegisterFuture&&) = default;
	UpdateRegisterFuture& operator=(UpdateRegisterFuture&&) = default;

private:
	void on_output_change(
		UNUSED const ABSTRACTFUTURE& future, UNUSED uint8_t* output_data, uint8_t* output_current) override
	{
		// Only one call expected, directly get output value and change it
		uint8_t value = *(output_current - 1);
		value |= set_mask_;
		get_input().data[2] = value;
	}

	uint8_t set_mask_;
};

class MyGroup : public AbstractFuturesGroup<ABSTRACTFUTURE>
{
	using PARENT = AbstractFuturesGroup<ABSTRACTFUTURE>;
public:
	MyGroup(FutureListener& listener) : PARENT{&listener}
	{
		PARENT::init({&f1_, &f2_});
	} 

	MyFuture& get_f1()
	{
		return f1_;
	}

	MyFuture& get_f2()
	{
		return f2_;
	}

private:
	MyFuture f1_{};
	MyFuture f2_{};
};

int main() __attribute__((OS_main));
int main()
{
	board::init();
	// Enable interrupts at startup time
	sei();

	// Initialize debugging output
	serial::hard::UATX<board::USART::USART0> uart{output_buffer};
	uart.begin(115200);
	ostream out = uart.out();
	out << boolalpha << showbase;

	FutureListener listener{out};

	// Start feeding future and check output
	MyFuture f1{&listener};
	out << F("set_future_value(0x11)") << endl;
	f1.set_future_value_(uint8_t(0x11));

	out << F("set_future_value(0x22)") << endl;
	f1.set_future_value_(uint8_t(0x22));

	out << F("set_future_value(0x33)") << endl;
	f1.set_future_value_(uint8_t(0x33));

	out << F("set_future_value(0x44)") << endl;
	f1.set_future_value_(uint8_t(0x44));

	out << F("f1.status() = ") << f1.status() << endl;
	uint32_t result = 0;
	out << F("f1.get(result) = ") << f1.get(result) << endl;
	out << F("result = ") << hex << result << endl;

	// Start feeding future, force error and check output
	MyFuture f2{&listener};
	f2.set_future_value_(uint8_t(0x55));
	f2.set_future_finish_();
	f2.set_future_error_(-10);
	out << F("f2.status() = ") << f2.status() << endl;
	out << F("f2.error() = ") << dec << f2.error() << endl;

	UpdateRegisterFuture f3{0xF7, 0x12};
	uint8_t data = 0;
	out << F("f3.get_storage_value_(data) = ") << f3.get_storage_value_(data) << endl;
	out << F("data = ") << hex << data << endl;
	out << F("f3.set_future_value_(0x40) = ") << f3.set_future_value_(uint8_t(0x40)) << endl;
	out << F("f3.get_storage_value_(data) = ") << f3.get_storage_value_(data) << endl;
	out << F("data = ") << hex << data << endl;
	out << F("f3.get_storage_value_(data) = ") << f3.get_storage_value_(data) << endl;
	out << F("data = ") << hex << data << endl;
	out << F("f3.status() = ") << f3.status() << endl;

	{
		// Group of futures
		out << F("Testing group of futures #1.1") << endl;
		MyGroup group{listener};
		out << F("group.status() = ") << group.status() << endl;
		out << F("set_future_value(0x11)") << endl;
		group.get_f1().set_future_value_(uint8_t(0x11));
		out << F("group.status() = ") << group.status() << endl;

		out << F("set_future_value(0x22)") << endl;
		group.get_f1().set_future_value_(uint8_t(0x22));
		out << F("group.status() = ") << group.status() << endl;

		out << F("set_future_value(0x33)") << endl;
		group.get_f1().set_future_value_(uint8_t(0x33));
		out << F("group.status() = ") << group.status() << endl;

		out << F("set_future_value(0x44)") << endl;
		group.get_f1().set_future_value_(uint8_t(0x44));
		out << F("group.status() = ") << group.status() << endl;

		out << F("f1.status() = ") << group.get_f1().status() << endl;
		result = 0;
		out << F("f1.get(result) = ") << group.get_f1().get(result) << endl;
		out << F("result = ") << hex << result << endl;

		out << F("Testing group of futures #1.2") << endl;
		group.get_f2().set_future_value_(uint8_t(0x55));
		out << F("group.status() = ") << group.status() << endl;
		group.get_f2().set_future_finish_();
		out << F("group.status() = ") << group.status() << endl;
		group.get_f2().set_future_error_(-10);
		out << F("group.status() = ") << group.status() << endl;
		out << F("f2.status() = ") << group.get_f2().status() << endl;
		out << F("f2.error() = ") << dec << group.get_f2().error() << endl;
		out << F("group.error() = ") << group.error() << endl;
	}

	{
		// Group of futures
		out << F("Testing group of futures #2.1") << endl;
		MyGroup group{listener};
		out << F("group.status() = ") << group.status() << endl;
		out << F("set_future_value(0x11)") << endl;
		group.get_f1().set_future_value_(uint8_t(0x11));
		out << F("group.status() = ") << group.status() << endl;

		out << F("set_future_value(0x22)") << endl;
		group.get_f1().set_future_value_(uint8_t(0x22));
		out << F("group.status() = ") << group.status() << endl;

		out << F("set_future_value(0x33)") << endl;
		group.get_f1().set_future_value_(uint8_t(0x33));
		out << F("group.status() = ") << group.status() << endl;

		out << F("set_future_value(0x44)") << endl;
		group.get_f1().set_future_value_(uint8_t(0x44));
		out << F("group.status() = ") << group.status() << endl;

		out << F("f1.status() = ") << group.get_f1().status() << endl;
		result = 0;
		out << F("f1.get(result) = ") << group.get_f1().get(result) << endl;
		out << F("result = ") << hex << result << endl;

		out << F("Testing group of futures #2.2") << endl;
		out << F("set_future_value(0x55)") << endl;
		group.get_f2().set_future_value_(uint8_t(0x55));
		out << F("group.status() = ") << group.status() << endl;
		out << F("set_future_value(0x66)") << endl;
		group.get_f2().set_future_value_(uint8_t(0x66));
		out << F("group.status() = ") << group.status() << endl;
		out << F("set_future_value(0x77)") << endl;
		group.get_f2().set_future_value_(uint8_t(0x77));
		out << F("group.status() = ") << group.status() << endl;
		out << F("set_future_value(0x88)") << endl;
		group.get_f2().set_future_value_(uint8_t(0x88));
		out << F("group.status() = ") << group.status() << endl;
		out << F("f2.status() = ") << group.get_f2().status() << endl;
		out << F("f2.error() = ") << dec << group.get_f2().error() << endl;
		out << F("group.error() = ") << group.error() << endl;
	}
}
