/*
 * This program is just for personal experiments here on C++ mixin for 
 * handling lifecycle of specific objects.
 */

// includes for the example program itself
#include <fastarduino/tests/assertions.h>
#include <fastarduino/uart.h>
#include <fastarduino/iomanip.h>
#include <fastarduino/flash.h>
#include <fastarduino/move.h>
#include <fastarduino/lifecycle.h>

// Register vector for UART (used for debug)
REGISTER_UATX_ISR(0)

using namespace streams;
using namespace lifecycle;

// Define types that output traces on each ctor/dtor/operator=
class Value
{
public:
	static void set_out(ostream& out)
	{
		out_ = &out;
	}
	Value(int val = 0) : val_{val}
	{
		trace('c');
	}
	~Value()
	{
		trace('d');
	}
	Value(const Value& that) : val_{that.val_}
	{
		trace('C');
	}
	Value(Value&& that) : val_{that.val_}
	{
		trace('M');
	}
	Value& operator=(const Value& that)
	{
		this->val_ = that.val_;
		trace('=');
		return *this;
	}
	Value& operator=(Value&& that)
	{
		this->val_ = that.val_;
		trace('m');
		return *this;
	}

	int val() const
	{
		return val_;
	}

	static ostream& out()
	{
		return *out_;
	}

protected:
	void trace(char method)
	{
		if (out_)
			*out_ << method << dec << val_ << ' ' << hex << this << endl;
	}

	static ostream* out_;
	int val_;
};

class SubValue : public Value
{
public:
	SubValue(int val = 0, int val2 = 0) : Value{val}, val2_{val2} {}
	~SubValue() = default;
	SubValue(const SubValue& that) = default;
	SubValue(SubValue&& that) = default;
	SubValue& operator=(const SubValue& that) = default;
	SubValue& operator=(SubValue&& that) = default;

	int val2() const
	{
		return val2_;
	}

private:
	int val2_;
};

static constexpr const uint8_t MAX_LC_SLOTS = 32;

template<typename T> static void check(ostream& out, AbstractLifeCycleManager& manager, const T& init)
{
	{
		out << F("0. Instance creation") << endl;
		LifeCycle<T> instance{init};
		assert(out, F("available_slots()"), MAX_LC_SLOTS, manager.available_());
		assert(out, F("id() after construction"), 0, instance.id());

		out << F("1. Registration") << endl;
		uint8_t id = manager.register_(instance);
		assert(out, F("id returned by register_()"), id);
		assert(out, F("id() after registration"), id, instance.id());
		assert(out, F("available_slots()"), MAX_LC_SLOTS - 1, manager.available_());

		out << F("2. Find") << endl;
		LifeCycle<T>* found = manager.find_<T>(id);
		assert(out, F("manager.find_(id)"), found != nullptr);
		assert(out, F("manager.find_(id)"), &instance, found);
		out << F("val=") << dec << found->val() << endl;

		// Check copy never compiles
		// LifeCycle<T> copy{instance};

		out << F("3. Move constructor") << endl;
		LifeCycle<T> move = std::move(instance);
		assert(out, F("original id() after registration"), 0, instance.id());
		assert(out, F("moved id() after registration"), id, move.id());
		assert(out, F("available_slots()"), MAX_LC_SLOTS - 1, manager.available_());

		out << F("4. Find after move") << endl;
		found = manager.find_<T>(id);
		assert(out, F("manager.find_(id)"), found != nullptr);
		assert(out, F("manager.find_(id)"), &move, found);
		out << F("val=") << dec << found->val() << endl;

		// Check copy never compiles
		// LifeCycle<T> copy2;
		// copy2 = move;

		out << F("5. Move assignment") << endl;
		LifeCycle<T> move2;
		move2 = std::move(move);
		assert(out, F("original id() after registration"), 0, move.id());
		assert(out, F("moved id() after registration"), id, move2.id());
		assert(out, F("available_slots()"), MAX_LC_SLOTS - 1, manager.available_());
	}

	// Check destruction
	out << F("6. Destruction") << endl;
	assert(out, F("available_slots()"), MAX_LC_SLOTS, manager.available_());
}

void check_proxies(ostream& out, AbstractLifeCycleManager& manager)
{
	Value v1{10};
	SubValue v2{20, 30};

	Proxy<Value> p1{v1};
	Proxy<Value> p2{v2};
	out << F("p1->val() ") << hex << &p1 << ' ' << dec << p1->val() << endl;
	out << F("p2->val() ") << hex << &p2 << ' ' << dec << p2->val() << endl;

	LifeCycle<Value> lc1{v1};
	assert(out, F("manager.register_(lc1)"), 1, manager.register_(lc1));
	assert(out, F("lc1.id()"), 1, lc1.id());
	LifeCycle<SubValue> lc2{v2};
	assert(out, F("manager.register_(lc2)"), 2, manager.register_(lc2));
	assert(out, F("lc2.id()"), 2, lc2.id());

	Proxy<Value> p3{lc1};
	out << F("p3.id=") << dec << p3.id()
		<< F(" p3.manager=") << hex << p3.manager() 
		<< F(" p3.dest=") << hex << p3.destination() << endl;
	Proxy<Value> p4{lc2};
	out << F("p4.id=") << dec << p4.id() 
		<< F(" p4.manager=") << hex << p4.manager()
		<< F(" p4.dest=") << hex << p4.destination() << endl;
	out << F("p3->val() ") << hex << &p3 << ' ' << dec << p3->val() << endl;
	out << F("p4->val() ") << hex << &p4 << ' ' << dec << p4->val() << endl;

	// This shall not compile
	// Proxy<SubValue> p5{lc1};
}

ostream* Value::out_ = nullptr;

static constexpr const uint8_t OUTPUT_BUFFER_SIZE = 128;
static char output_buffer[OUTPUT_BUFFER_SIZE];

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

	out << F("Starting...") << endl;

	Value::set_out(out);
	out << F("Create constant Value first") << endl;
	const Value VAL0 = Value{123};

	// Create manager
	out << F("Instantiate LifeCycleManager") << endl;
	LifeCycleManager<MAX_LC_SLOTS> manager;
	// Check available slots
	assert(out, F("available_slots()"), MAX_LC_SLOTS, manager.available_());

	// Check different types T (int, struct, with/without dtor/ctor/op=...)
	// check<Value>(out, manager, VAL0);

	check_proxies(out, manager);
}