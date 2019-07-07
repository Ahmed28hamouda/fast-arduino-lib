//   Copyright 2016-2019 Jean-Francois Poilpret
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

/// @cond api

/**
 * @file 
 * Software-emulated serial API.
 */
#ifndef SOFTUART_HH
#define SOFTUART_HH

#include "boards/board.h"
#include "interrupts.h"
#include "utilities.h"
#include "uart_commons.h"
#include "streams.h"
#include "gpio.h"
#include "pci.h"
#include "int.h"

/**
 * Register the necessary ISR (Interrupt Service Routine) for an serial::soft::UARX
 * to work correctly. This applies to an `UARX` which @p RX pin is a PCINT pin.
 * @param RX the `board::InterruptPin` used as RX for the UARX
 * @param PCI_NUM the number of the `PCINT` vector for the given @p RX pin
 */
#define REGISTER_UARX_PCI_ISR(RX, PCI_NUM)                        \
	ISR(CAT3(PCINT, PCI_NUM, _vect))                              \
	{                                                             \
		serial::soft::isr_handler::check_uarx_pci<PCI_NUM, RX>(); \
	}

/**
 * Register the necessary ISR (Interrupt Service Routine) for an serial::soft::UARX
 * to work correctly. This applies to an `UARX` which @p RX pin is an External INT pin.
 * @param RX the `board::ExternalInterruptPin` used as RX for the UARX
 * @param INT_NUM the number of the `INT` vector for the given @p RX pin
 */
#define REGISTER_UARX_INT_ISR(RX, INT_NUM)                        \
	ISR(CAT3(INT, INT_NUM, _vect))                                \
	{                                                             \
		serial::soft::isr_handler::check_uarx_int<INT_NUM, RX>(); \
	}

/**
 * Register the necessary ISR (Interrupt Service Routine) for an serial::soft::UART
 * to work correctly. This applies to an `UART` which @p RX pin is a PCINT pin.
 * @param RX the `board::InterruptPin` used as RX for the UART
 * @param TX the `board::DigitalPin` used as TX for the UART
 * @param PCI_NUM the number of the `PCINT` vector for the given @p RX pin
 */
#define REGISTER_UART_PCI_ISR(RX, TX, PCI_NUM)                        \
	ISR(CAT3(PCINT, PCI_NUM, _vect))                                  \
	{                                                                 \
		serial::soft::isr_handler::check_uart_pci<PCI_NUM, RX, TX>(); \
	}

/**
 * Register the necessary ISR (Interrupt Service Routine) for an serial::soft::UART
 * to work correctly. This applies to an `UART` which @p RX pin is an External INT pin.
 * @param RX the `board::ExternalInterruptPin` used as RX for the UART
 * @param TX the `board::DigitalPin` used as TX for the UART
 * @param INT_NUM the number of the `INT` vector for the given @p RX pin
 */
#define REGISTER_UART_INT_ISR(RX, TX, INT_NUM)                        \
	ISR(CAT3(INT, INT_NUM, _vect))                                    \
	{                                                                 \
		serial::soft::isr_handler::check_uart_int<INT_NUM, RX, TX>(); \
	}

//FIXME Handle begin/end properly in relation to current queue content
/**
 * Defines API types used by software UART features.
 * This API is available to all MCU, even those that do not have hardware UART,
 * hence even ATtiny MCU are supported.
 * IMPORTANT! Note that software-emulated UART cannot be as fast as hardware UART,
 * for that reason a maximum rate of 115'200bps is supported, preferrably with 2 
 * stop bits (depending on the sending device, UART reception may lose bits if only
 * one stop bit is used).
 * 
 * @sa serial::hard
 */
namespace serial::soft
{
	/// @cond notdocumented
	class AbstractUATX
	{
	public:
		/**
		 * Get the formatted output stream used to send content through this serial
		 * transmitter.
		 */
		streams::ostream out()
		{
			return streams::ostream(out_());
		}

	protected:
		using CALLBACK = streams::ostreambuf::CALLBACK;

		template<uint8_t SIZE_TX> 
		explicit AbstractUATX(char (&output)[SIZE_TX], CALLBACK callback, void* arg)
		: obuf_{output, callback, arg} {}

		void compute_times(uint32_t rate, StopBits stop_bits);
		static Parity calculate_parity(Parity parity, uint8_t value);

		streams::ostreambuf& out_()
		{
			return obuf_;
		}

		void check_overflow(Errors& errors)
		{
			errors.queue_overflow = obuf_.overflow();
		}

		template<board::DigitalPin DPIN> void write(Parity parity, uint8_t value)
		{
			synchronized write_<DPIN>(parity, value);
		}
		template<board::DigitalPin DPIN> void write_(Parity parity, uint8_t value);

	private:
		// NOTE declaring obuf_ first instead of last optimizes code size (4 bytes)
		streams::ostreambuf obuf_;

	protected:
		// Various timing constants based on rate
		uint16_t interbit_tx_time_;
		uint16_t start_bit_tx_time_;
		uint16_t stop_bit_tx_time_;
	};

	template<board::DigitalPin DPIN> void AbstractUATX::write_(Parity parity, uint8_t value)
	{
		using TX = gpio::FastPinType<DPIN>;
		// Pre-calculate all what we need: parity bit
		Parity parity_bit = calculate_parity(parity, value);

		// Write start bit
		TX::clear();
		// Wait before starting actual value transmission
		_delay_loop_2(start_bit_tx_time_);
		for (uint8_t bit = 0; bit < 8; ++bit)
		{
			if (value & 0x01)
				TX::set();
			else
				TX::clear();
			value >>= 1;
			_delay_loop_2(interbit_tx_time_);
		}
		// Add parity if needed
		if (parity_bit != Parity::NONE)
		{
			if (parity_bit == parity)
				TX::clear();
			else
				TX::set();
			_delay_loop_2(interbit_tx_time_);
		}
		// Add stop bit
		TX::set();
		_delay_loop_2(stop_bit_tx_time_);
	}
	/// @endcond

	/**
	 * Software-emulated serial transmitter API.
	 * 
	 * @tparam TX_ the `board::DigitalPin` to which transmitted signal is sent
	 * 
	 * @sa UARX
	 * @sa UART
	 */
	template<board::DigitalPin TX_> class UATX : public AbstractUATX, public UARTErrors
	{
	private:
		using THIS = UATX<TX_>;

	public:
		/** The `board::DigitalPin` to which transmitted signal is sent */
		static constexpr const board::DigitalPin TX = TX_;

		/**
		 * Construct a new software serial transmitter and provide it with a
		 * buffer for payload transmission.
		 * @param output an array of characters used by this transmitter to
		 * buffer output during transmission
		 */
		template<uint8_t SIZE_TX> explicit UATX(char (&output)[SIZE_TX])
		: AbstractUATX{output, THIS::on_put, this}, tx_{gpio::PinMode::OUTPUT, true} {}

		/**
		 * Enable the transmitter. 
		 * This is needed before any transmission can take place.
		 * Once called, it is possible to push content to `out()`,
		 * which will be then transmitted through the serial connection.
		 * 
		 * @param rate the transmission rate in bits per second (bps)
		 * @param parity the kind of parity check used by transmission
		 * @param stop_bits the number of stop bits used by transmission
		 */
		void begin(uint32_t rate, Parity parity = Parity::NONE, StopBits stop_bits = StopBits::ONE)
		{
			parity_ = parity;
			compute_times(rate, stop_bits);
			//FIXME if queue is not empty, we should process it until everything is written (or clear it?)
		}

		/**
		 * Stop all transmissions.
		 * Once called, it is possible to re-enable transmission again by
		 * calling `begin()`.
		 */
		void end()
		{
			//FIXME if queue not empty we should:
			// - prevent pushing to it (how?)
			// - flush it completely
			// - enable pushing again?
		}

	private:
		static void on_put(void* arg)
		{
			THIS& target = *((THIS*) arg);
			//FIXME we should write ONLY if UAT is active (begin() has been called and not end())
			target.check_overflow(target.errors());
			char value;
			while (target.out_().queue().pull(value)) target.write<TX>(target.parity_, uint8_t(value));
		}

		Parity parity_;
		typename gpio::FastPinType<TX>::TYPE tx_;
	};

	/// @cond notdocumented
	class AbstractUARX
	{
	public:
		/**
		 * Get the formatted input stream used to read content received through
		 * this serial transmitter.
		 */
		streams::istream in()
		{
			return streams::istream(in_());
		}

	protected:
		template<uint8_t SIZE_RX> explicit AbstractUARX(char (&input)[SIZE_RX]) : ibuf_{input} {}

		streams::istreambuf& in_()
		{
			return ibuf_;
		}

		void compute_times(uint32_t rate, bool has_parity, StopBits stop_bits);

		template<board::DigitalPin DPIN> void pin_change(Parity parity, Errors& errors);

	private:
		// NOTE declaring ibuf_ first instead of last optimizes code size (2 bytes)
		streams::istreambuf ibuf_;

	protected:
		// Various timing constants based on rate
		uint16_t interbit_rx_time_;
		uint16_t start_bit_rx_time_;
	};

	template<board::DigitalPin DPIN> void AbstractUARX::pin_change(Parity parity, Errors& errors)
	{
		using RX = gpio::FastPinType<DPIN>;
		// Check RX is low (start bit)
		if (RX::value()) return;
		uint8_t value = 0;
		bool odd = false;
		errors.has_errors = 0;
		// Wait for start bit to finish
		_delay_loop_2(start_bit_rx_time_);
		// Read first 7 bits
		for (uint8_t i = 0; i < 8; ++i)
		{
			if (RX::value())
			{
				value |= 0x80;
				odd = !odd;
			}
			if (i < 7)
				value >>= 1;
			_delay_loop_2(interbit_rx_time_);
		}

		if (parity != Parity::NONE)
		{
			// Check parity bit
			bool parity_bit = (parity == Parity::ODD ? !odd : odd);
			// Check parity bit
			errors.parity_error = (RX::value() != parity_bit);
			_delay_loop_2(interbit_rx_time_);
		}

		// Check we receive a stop bit
		if (!RX::value())
			errors.frame_error = true;
		// Push value if no error
		if (errors.has_errors == 0)
			errors.queue_overflow = !in_().queue().push_(char(value));
	}
	/// @endcond

	/// @cond notdocumented
	template<typename T, T IRQ> class UARX {};
	/// @endcond

	/** @sa UARX_EXT */
	template<board::ExternalInterruptPin RX_>
	class UARX<board::ExternalInterruptPin, RX_> : public AbstractUARX, public UARTErrors
	{
	public:
		/**
		 * The `board::DigitalPin` which shall receive serial signal; this
		 * must be either an External INT pin (`board::ExternalInterruptPin`) or a 
		 * PinChangeInterrupt pin (`board::InterruptPin`).
		 */
		static constexpr const board::DigitalPin RX = board::EXT_PIN<RX_>();

		/**
		 * The interrupt::INTSignal type for `RX_` pin, if it is a
		 * External Interrupt pin. This type is used in `begin()` call.
		 * @sa begin()
		 */
		using INT_TYPE = typename interrupt::INTSignal<RX_>;

		/**
		 * Construct a new software serial receiver and provide it with a
		 * buffer for interrupt-based reception.
		 * Reception is asynchronous.
		 * @param input an array of characters used by this receiver to
		 * store content received through serial line, buffered until read through
		 * `in()`.
		 * @sa REGISTER_UART_INT_ISR()
		 */
		template<uint8_t SIZE_RX> 
		explicit UARX(char (&input)[SIZE_RX]) : AbstractUARX{input}, rx_{gpio::PinMode::INPUT}
		{
			interrupt::register_handler(*this);
		}

		/**
		 * Enable the receiver. 
		 * This is needed before any reception can take place.
		 * Once called, it is possible to read content, received through serial
		 * connection, by using `in()`.
		 * 
		 * @param enabler the `interrupt::INTSignal` for the RX pin; it is used to
		 * enable interrupts on that pin.
		 * @param rate the transmission rate in bits per second (bps)
		 * @param parity the kind of parity check used by transmission
		 * @param stop_bits the number of stop bits used by transmission
		 */
		void begin(INT_TYPE& enabler, uint32_t rate, Parity parity = Parity::NONE, StopBits stop_bits = StopBits::ONE)
		{
			parity_ = parity;
			int_ = &enabler;
			AbstractUARX::compute_times(rate, parity != Parity::NONE, stop_bits);
			int_->enable();
		}
		
		/**
		 * Stop reception.
		 * Once called, it is possible to re-enable reception again by
		 * calling `begin()`.
		 */
		void end()
		{
			int_->disable();
		}

	private:
		void on_pin_change()
		{
			this->pin_change<RX>(parity_, errors());
			// Clear PCI interrupt to remove pending PCI occurred during this method and to detect next start bit
			int_->clear_();
		}

		Parity parity_;
		typename gpio::FastPinType<RX>::TYPE rx_;
		INT_TYPE* int_;
		friend struct isr_handler;
	};

	/// @cond notdocumented
	template<typename T, T IRQ, board::DigitalPin TX> class UART {};
	/// @endcond

	/** @sa UART_EXT */
	template<board::ExternalInterruptPin RX_, board::DigitalPin TX_>
	class UART<board::ExternalInterruptPin, RX_, TX_> : public AbstractUARX, public AbstractUATX, public UARTErrors
	{
	private:
		using THIS = UART<board::ExternalInterruptPin, RX_, TX_>;

	public:
		/** The `board::DigitalPin` to which transmitted signal is sent */
		static constexpr const board::DigitalPin TX = TX_;
		/**
		 * The `board::DigitalPin` which shall receive serial signal.
		 */
		static constexpr const board::DigitalPin RX = board::EXT_PIN<RX_>();

		/**
		 * The interrupt::INTSignal type for `RX_` pin, if it is a
		 * External Interrupt pin. This type is used in `begin()` call.
		 * @sa begin()
		 */
		using INT_TYPE = typename interrupt::INTSignal<RX_>;

		/**
		 * Construct a new software serial receiver/transceiver and provide it 
		 * with 2 buffers, one for interrupt-based reception, one for transmission.
		 * 
		 * @param input an array of characters used by this receiver to
		 * store content received through serial line, buffered until read through
		 * `in()`.
		 * @param output an array of characters used by this transmitter to
		 * buffer output during transmission.
		 */
		template<uint8_t SIZE_RX, uint8_t SIZE_TX>
		explicit UART(char (&input)[SIZE_RX], char (&output)[SIZE_TX])
		:	AbstractUARX{input}, AbstractUATX{output, THIS::on_put, this}, 
			tx_{gpio::PinMode::OUTPUT, true}, rx_{gpio::PinMode::INPUT}
		{
			interrupt::register_handler(*this);
		}

		/**
		 * Enable the receiver/transceiver. 
		 * This is needed before any transmission or reception can take place.
		 * Once called, it is possible to send and receive content through serial
		 * connection, by using `in()` for reading and `out()` for writing.
		 * 
		 * @param enabler the `interrupt::INTSignal` for the RX pin; it is used to
		 * enable interrupts on that pin.
		 * @param rate the transmission rate in bits per second (bps)
		 * @param parity the kind of parity check used by transmission
		 * @param stop_bits the number of stop bits used by transmission
		 * 
		 * @sa begin(INT_TYPE&, uint32_t, Parity, StopBits)
		 */
		void begin(INT_TYPE& enabler, uint32_t rate, Parity parity = Parity::NONE, StopBits stop_bits = StopBits::ONE)
		{
			parity_ = parity;
			int_ = &enabler;
			AbstractUARX::compute_times(rate, parity != Parity::NONE, stop_bits);
			AbstractUATX::compute_times(rate, stop_bits);
			int_->enable();
		}

		/**
		 * Stop all transmissions and receptions.
		 * Once called, it is possible to re-enable transmission and reception 
		 * again by calling `begin()`.
		 */
		void end()
		{
			int_->disable();
		}

	private:
		static void on_put(void* arg)
		{
			THIS& target = *((THIS*) arg);
			//FIXME we should write ONLY if UAT is active (begin() has been called and not end())
			target.check_overflow(target.errors());
			char value;
			while (target.out_().queue().pull(value)) target.write<TX>(target.parity_, uint8_t(value));
		}

		void on_pin_change()
		{
			this->pin_change<RX>(parity_, errors());
			// Clear PCI interrupt to remove pending PCI occurred during this method and to detect next start bit
			int_->clear_();
		}

		Parity parity_;
		typename gpio::FastPinType<TX>::TYPE tx_;
		typename gpio::FastPinType<RX>::TYPE rx_;
		INT_TYPE* int_;
		friend struct isr_handler;
	};

	/** @sa UARX_PCI */
	template<board::InterruptPin RX_> class UARX<board::InterruptPin, RX_> : public AbstractUARX, public UARTErrors
	{
	public:
		/**
		 * The `board::DigitalPin` which shall receive serial signal; this
		 * must be either an External INT pin (`board::ExternalInterruptPin`) or a 
		 * PinChangeInterrupt pin (`board::InterruptPin`).
		 */
		static constexpr const board::DigitalPin RX = board::PCI_PIN<RX_>();

		/**
		 * The interrupt::PCISignal type for `RX_` pin, if it is a
		 * PinChangeInterrupt pin. This type is used in `begin()` call.
		 * @sa begin()
		 */
		using PCI_TYPE = typename interrupt::PCIType<RX_>::TYPE;

		/**
		 * Construct a new software serial receiver and provide it with a
		 * buffer for interrupt-based reception.
		 * Reception is asynchronous.
		 * @param input an array of characters used by this receiver to
		 * store content received through serial line, buffered until read through
		 * `in()`.
		 * @sa REGISTER_UART_PCI_ISR()
		 */
		template<uint8_t SIZE_RX>
		explicit UARX(char (&input)[SIZE_RX]) : AbstractUARX{input}, rx_{gpio::PinMode::INPUT}
		{
			interrupt::register_handler(*this);
		}

		/**
		 * Enable the receiver. 
		 * This is needed before any reception can take place.
		 * Once called, it is possible to read content, received through serial
		 * connection, by using `in()`.
		 * 
		 * @param enabler the `interrupt::PCISignal` for the RX pin; it is used to
		 * enable interrupts on that pin.
		 * @param rate the transmission rate in bits per second (bps)
		 * @param parity the kind of parity check used by transmission
		 * @param stop_bits the number of stop bits used by transmission
		 * 
		 * @sa begin(INT_TYPE&, uint32_t, Parity, StopBits)
		 */
		void begin(PCI_TYPE& enabler, uint32_t rate, Parity parity = Parity::NONE, StopBits stop_bits = StopBits::ONE)
		{
			parity_ = parity;
			pci_ = &enabler;
			AbstractUARX::compute_times(rate, parity != Parity::NONE, stop_bits);
			pci_->template enable_pin<RX_>();
		}

		/**
		 * Stop reception.
		 * Once called, it is possible to re-enable reception again by
		 * calling `begin()`.
		 */
		void end()
		{
			pci_->template disable_pin<RX_>();
		}

	private:
		void on_pin_change()
		{
			this->pin_change<RX>(parity_, errors());
			// Clear PCI interrupt to remove pending PCI occurred during this method and to detect next start bit
			pci_->clear_();
		}

		Parity parity_;
		typename gpio::FastPinType<RX>::TYPE rx_;
		PCI_TYPE* pci_;
		friend struct isr_handler;
	};

	/** @sa UART_PCI */
	template<board::InterruptPin RX_, board::DigitalPin TX_>
	class UART<board::InterruptPin, RX_, TX_> : public AbstractUARX, public AbstractUATX, public UARTErrors
	{
	private:
		using THIS = UART<board::InterruptPin, RX_, TX_>;

	public:
		/** The `board::DigitalPin` to which transmitted signal is sent */
		static constexpr const board::DigitalPin TX = TX_;
		/**
		 * The `board::DigitalPin` which shall receive serial signal.
		 */
		static constexpr const board::DigitalPin RX = board::PCI_PIN<RX_>();

		/**
		 * The interrupt::PCISignal type for `RX_` pin, if it is a
		 * PinChangeInterrupt pin. This type is used in `begin()` call.
		 * @sa begin()
		 */
		using PCI_TYPE = typename interrupt::PCIType<RX_>::TYPE;

		/**
		 * Construct a new software serial receiver/transceiver and provide it 
		 * with 2 buffers, one for interrupt-based reception, one for transmission.
		 * 
		 * @param input an array of characters used by this receiver to
		 * store content received through serial line, buffered until read through
		 * `in()`.
		 * @param output an array of characters used by this transmitter to
		 * buffer output during transmission.
		 */
		template<uint8_t SIZE_RX, uint8_t SIZE_TX>
		explicit UART(char (&input)[SIZE_RX], char (&output)[SIZE_TX])
		:	AbstractUARX{input}, AbstractUATX{output, THIS::on_put, this}, 
			tx_{gpio::PinMode::OUTPUT, true}, rx_{gpio::PinMode::INPUT}
		{
			interrupt::register_handler(*this);
		}

		/**
		 * Enable the receiver/transceiver. 
		 * This is needed before any transmission or reception can take place.
		 * Once called, it is possible to send and receive content through serial
		 * connection, by using `in()` for reading and `out()` for writing.
		 * 
		 * @param enabler the `interrupt::PCISignal` for the RX pin; it is used to
		 * enable interrupts on that pin.
		 * @param rate the transmission rate in bits per second (bps)
		 * @param parity the kind of parity check used by transmission
		 * @param stop_bits the number of stop bits used by transmission
		 * 
		 * @sa begin(INT_TYPE&, uint32_t, Parity, StopBits)
		 */
		void begin(PCI_TYPE& enabler, uint32_t rate, Parity parity = Parity::NONE, StopBits stop_bits = StopBits::ONE)
		{
			parity_ = parity;
			pci_ = &enabler;
			AbstractUARX::compute_times(rate, parity != Parity::NONE, stop_bits);
			AbstractUATX::compute_times(rate, stop_bits);
			pci_->template enable_pin<RX_>();
		}

		/**
		 * Stop all transmissions and receptions.
		 * Once called, it is possible to re-enable transmission and reception 
		 * again by calling `begin()`.
		 */
		void end()
		{
			pci_->template disable_pin<RX_>();
		}

	private:
		static void on_put(void* arg)
		{
			THIS& target = *((THIS*) arg);
			//FIXME we should write ONLY if UAT is active (begin() has been called and not end())
			target.check_overflow(target.errors());
			char value;
			while (target.out_().queue().pull(value)) target.write<TX>(target.parity_, uint8_t(value));
		}

		void on_pin_change()
		{
			this->pin_change<RX>(parity_, errors());
			// Clear PCI interrupt to remove pending PCI occurred during this method and to detect next start bit
			pci_->clear_();
		}

		Parity parity_;
		typename gpio::FastPinType<TX>::TYPE tx_;
		typename gpio::FastPinType<RX>::TYPE rx_;
		PCI_TYPE* pci_;
		friend struct isr_handler;
	};

	// Useful type aliases
	//=====================

	/**
	 * Software-emulated serial receiver API.
	 * For this API to be fully functional, you must register the right ISR in your
	 * program, through `REGISTER_UART_INT_ISR()`.
	 * 
	 * @tparam RX_ the `board::ExternalInterruptPin` which shall receive serial signal
	 * 
	 * @sa REGISTER_UART_INT_ISR()
	 * @sa UARX
	 * @sa UATX
	 * @sa UART
	 */
	template<board::ExternalInterruptPin RX_> using UARX_EXT = UARX<board::ExternalInterruptPin, RX_>;

	/**
	 * Software-emulated serial receiver API.
	 * For this API to be fully functional, you must register the right ISR in your
	 * program, through `REGISTER_UART_PCI_ISR()`.
	 * 
	 * @tparam RX_ the `board::InterruptPin` which shall receive serial signal
	 * 
	 * @sa REGISTER_UART_PCI_ISR()
	 * @sa UARX
	 * @sa UATX
	 * @sa UART
	 */
	template<board::InterruptPin RX_> using UARX_PCI = UARX<board::InterruptPin, RX_>;

	/**
	 * Software-emulated serial receiver/transceiver API.
	 * For this API to be fully functional, you must register the right ISR in your
	 * program, through `REGISTER_UART_INT_ISR()`.
	 * 
	 * @tparam RX_ the `board::ExternalInterruptPin` which shall receive serial signal
	 * @tparam TX_ the `board::DigitalPin` to which transmitted signal is sent
	 * 
	 * @sa REGISTER_UART_INT_ISR()
	 * @sa UART
	 * @sa UATX
	 * @sa UARX
	 */
	template<board::ExternalInterruptPin RX_, board::DigitalPin TX_>
	using UART_EXT = UART<board::ExternalInterruptPin, RX_, TX_>;

	/**
	 * Software-emulated serial receiver/transceiver API.
	 * For this API to be fully functional, you must register the right ISR in your
	 * program, through `REGISTER_UART_PCI_ISR()`.
	 * 
	 * @tparam RX_ the `board::InterruptPin` which shall receive serial signal
	 * @tparam TX_ the `board::DigitalPin` to which transmitted signal is sent
	 * 
	 * @sa REGISTER_UART_PCI_ISR()
	 * @sa UART
	 * @sa UATX
	 * @sa UARX
	 */
	template<board::InterruptPin RX_, board::DigitalPin TX_>
	using UART_PCI = UART<board::InterruptPin, RX_, TX_>;

	/// @cond notdocumented
	struct isr_handler
	{
		template<uint8_t PCI_NUM_, board::InterruptPin RX_> static void check_uarx_pci()
		{
			interrupt::isr_handler_pci::check_pci_pins<PCI_NUM_, RX_>();
			using UARX = serial::soft::UARX_PCI<RX_>;
			interrupt::HandlerHolder<UARX>::handler()->on_pin_change();
		}

		template<uint8_t INT_NUM_, board::ExternalInterruptPin RX_> static void check_uarx_int()
		{
			interrupt::isr_handler_int::check_int_pin<INT_NUM_, RX_>();
			using UARX = serial::soft::UARX_EXT<RX_>;
			interrupt::HandlerHolder<UARX>::handler()->on_pin_change();
		}

		template<uint8_t PCI_NUM_, board::InterruptPin RX_, board::DigitalPin TX_> static void check_uart_pci()
		{
			interrupt::isr_handler_pci::check_pci_pins<PCI_NUM_, RX_>();
			using UART = serial::soft::UART_PCI<RX_, TX_>;
			interrupt::HandlerHolder<UART>::handler()->on_pin_change();
		}

		template<uint8_t INT_NUM_, board::ExternalInterruptPin RX_, board::DigitalPin TX_> static void check_uart_int()
		{
			interrupt::isr_handler_int::check_int_pin<INT_NUM_, RX_>();
			using UART = serial::soft::UART_EXT<RX_, TX_>;
			interrupt::HandlerHolder<UART>::handler()->on_pin_change();
		}
	};
	/// @endcond
}

#endif /* SOFTUART_HH */
/// @endcond
