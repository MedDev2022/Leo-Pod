/*****************************************************************************
**																			**
**	 Name: 	Base Classes for a Communication libary							**
**																			**
*****************************************************************************/

#ifndef SRC_COMM_H_
#define SRC_COMM_H_

#include <cstdint>
#include <vector>
#include <cstring>

namespace comm {


/**
 * Device IDs for inter-device communication
 */
enum class DeviceID : uint8_t {
	HOST    = 0x10,
	LEOPOD  = 0x70,
	DAYCAM  = 0x21,
	IRAY    = 0x22,
	RPLENS  = 0x23,
	LRF     = 0x24
};
/**
 * Get device name string from ID
 */
const char* getDeviceName(DeviceID id);
const char* getDeviceName(uint8_t id);

/**
 * Global device ID (set during initialization)
 */
extern uint8_t MY_ID;


/**
 * Protocol constants
 */
constexpr uint8_t HEADER_BYTE = 0xAA;
constexpr uint8_t FOOTER_BYTE = 0x55;
constexpr uint8_t MAX_PAYLOAD_SIZE = 256;
/**
 * Calculate a CRC for a given array, starting from an offset
 *
 * @param	aData			a data array
 * @param	ulOffset		an offset
 * @param	ulLength		a length of a data to calculate CRC
 * @return					CRC value
 */
uint8_t CRC8(const uint8_t aData[], uint32_t ulOffset, uint32_t ulLength);
/**
 * Message class for device-to-device communication
 *
 * Protocol format:
 * [Header] [SrcID] [DestID] [OpCode] [Addr] [Length] [Payload...] [CRC] [Footer]
 *   0xAA     1B      1B       1B       1B      1B      0-255B        1B    0x55
 */

class Message {
public:
	// Constructors
	Message();
	Message(uint8_t destID, uint8_t opCode, uint8_t addr, const uint8_t* data, uint8_t length);
	Message(DeviceID source, DeviceID destination, uint8_t opCode);
	Message(DeviceID source, DeviceID destination, uint8_t opCode, uint8_t addr,
	        const uint8_t* data, uint8_t length);

	// Validation
	bool isForMe(uint8_t MY_ID) const;
	bool isForMe(DeviceID myID) const;
	bool isOK() const;
	bool isValid() const;

	// Getters
	uint8_t getOpCode() const { return m_opCode; }
	uint8_t getDestID() const { return m_destID; }
	uint8_t getSrcID() const { return m_srcID; }
	uint8_t getAddr() const { return m_addr; }
	uint8_t getLength() const { return m_length; }
	uint8_t getHeader() const { return m_Header; }
	uint8_t getFooter() const { return m_Footer; }
	uint8_t getCRC() const { return m_dataCRC; }

	DeviceID getSource() const { return static_cast<DeviceID>(m_srcID); }
	DeviceID getDestination() const { return static_cast<DeviceID>(m_destID); }

	// Setters
	void setSource(uint8_t srcID) { m_srcID = srcID; }
	void setSource(DeviceID srcID) { m_srcID = static_cast<uint8_t>(srcID); }
	void setDestination(uint8_t destID) { m_destID = destID; }
	void setDestination(DeviceID destID) { m_destID = static_cast<uint8_t>(destID); }
	void setOpCode(uint8_t opCode) { m_opCode = opCode; }
	void setAddr(uint8_t addr) { m_addr = addr; }
	void setPayload(const uint8_t* data, uint8_t length);
	void setPayload(const std::vector<uint8_t>& data);
	void setCRC(uint8_t crc) { m_dataCRC = crc; }
	void setFooter(uint8_t footer) { m_Footer = footer; }

	// Payload access
	std::vector<uint8_t>& payload() { return m_payload; }
	const std::vector<uint8_t>& payload() const { return m_payload; }
	const uint8_t* data() const { return m_payload.data(); }

	// Message building
	size_t size() const;
	size_t totalSize() const;  // Including header and footer
	uint8_t* rawBuffer(uint8_t* buffer) const;
	std::vector<uint8_t> build() const;
	void updateCRC();  // Recalculate and update CRC

	// Message parsing
	bool parse(const uint8_t* buffer, size_t length);
	static bool verify(const uint8_t* buffer, size_t length);

	// Expected message length based on length field
	static size_t expectedLength(const uint8_t* buffer, size_t currentLength);

	// CRC calculation
	uint8_t calcCRC(const uint8_t* data, size_t length) const;
	static uint8_t calculateCRC(const uint8_t* buffer, size_t length);

	// Protocol constants
		static constexpr size_t HEADER_SIZE = 6;  // Header + SrcID + DestID + OpCode + Addr + Length
		static constexpr size_t OVERHEAD = 8;     // Header + fields + CRC + Footer (no payload)
		static constexpr size_t MIN_MESSAGE_SIZE = 8;  // Minimum valid message

		// Public members (for compatibility)
		uint8_t m_Header;
		uint8_t m_srcID;
		uint8_t m_destID;
		uint8_t m_opCode;
		uint8_t m_addr;
		uint8_t m_length;
		std::vector<uint8_t> m_payload;
		uint8_t m_dataCRC;
		uint8_t m_Footer;
	};

/**
	 * Fluent builder for creating messages
	 *
	 * Example:
	 *   auto msg = MessageBuilder(DeviceID::HOST, DeviceID::DAYCAM)
	 *                .opCode(0x09)
	 *                .payload({0x0E, 0x0E})
	 *                .build();
	 */
	class MessageBuilder {
	public:
		MessageBuilder(DeviceID source, DeviceID destination);
		MessageBuilder(uint8_t source, uint8_t destination);

		MessageBuilder& opCode(uint8_t code);
		MessageBuilder& address(uint8_t addr);
		MessageBuilder& payload(const std::vector<uint8_t>& data);
		MessageBuilder& payload(const uint8_t* data, uint8_t length);

		std::vector<uint8_t> build();
		Message buildMessage();

	private:
		Message message_;
	};

	// ========================================================================
	// FILE TRANSFER CLASSES (Kept for compatibility)
	// ========================================================================

	/**
	 * A base class for a basic file communication unit.
	 */
	class Segment {
		friend class File;
	public:
		static const uint16_t OVERHEAD_SIZE = 12;
		static const uint16_t DATA_OFFSET = 11;
		static const uint16_t DATA_SIZE = 1024;
		static const uint16_t SIZE_BYTES = DATA_SIZE + OVERHEAD_SIZE; // 1040;

		/**
		 * Check if the segment dest ID matches current device ID
		 *
		 * @return					true: match
		 */
		bool isForMe() const;

		/**
		 * Check if the segment passes a CRC test (both header and data)
		 *
		 * @return					true: CRC passed
		 */
		bool isOK() const;

		/**
		 * Get a file number to which the segment belongs
		 *
		 * @return					file number
		 */
		uint16_t getFileNumber() const { return m_usFileNumber; }

		/**
		 * Get a segment size in bytes, that is a constant equal to SIZE_BYTES
		 *
		 * @return					message size in bytes
		 */
		uint32_t size() const { return SIZE_BYTES; }

	protected:
		uint8_t m_srcID;
		uint8_t m_destID;
		uint16_t m_usFileNumber;
		uint32_t m_ulFileLength;
		uint16_t m_usNumber;
		uint8_t m_headerCRC;
		uint8_t m_aData[DATA_SIZE];
		uint8_t m_dataCRC;
		uint16_t m_length;
	};

	/**
	 * A class to manage file transfer
	 */
	class File {
	public:
		File();
		File(uint8_t* pFileBuffer);
		File(uint8_t destID, uint16_t usNumber, void* pData, uint32_t ulDataSize);

		bool getNextSegment(Segment& seg);
		uint8_t getDestID() const { return m_destID; }
		uint8_t getSrcID() const { return m_srcID; }
		uint32_t getDataSize() const;
		uint16_t getNumber() const { return m_usNumber; }
		uint8_t* getData() { return m_pData; }
		uint16_t getSegmentNumber() { return (m_usNextSegmentNumber - 1); }
		bool append(Segment* seg);
		bool segmentIsLast() const;
		bool segmentIsFirst() const;

	private:
		uint8_t m_destID;
		uint8_t m_srcID;
		uint16_t m_usNumber;
		uint16_t m_usTotalNumberOfSegments;
		uint16_t m_usSizeOfTheLastSegment;
		uint32_t m_ulLength;
		uint16_t m_usNextSegmentNumber;
		uint8_t* m_pData;
	};

	/**
	 * A class template for the most basic transfer (simple transaction)
	 */
	template <typename SendingData, typename ReceivingData>
	class Transfer {
	private:
		SendingData* m_pSendingData;
		ReceivingData* m_pReceivingData;

	public:
		Transfer()
			: m_pSendingData(0), m_pReceivingData(0) {}

		Transfer(SendingData* pSendingData, ReceivingData* pReceivingData)
			: m_pSendingData(pSendingData), m_pReceivingData(pReceivingData) {}

		template <typename Port>
		void start(Port& port) {
			if (m_pSendingData)
				port->send(m_pSendingData, m_pSendingData->size());
			if (m_pReceivingData)
				port->receive(m_pReceivingData, m_pReceivingData->size());
		}

		template <typename Port>
		bool run(Port& port) {
			if (m_pReceivingData) {
				if (port->received()) {
					if (m_pReceivingData->isForMe() && m_pReceivingData->isOK())
						return true;
					else
						port->receive();
				}
			}
			return false;
		}

		SendingData* sending() { return m_pSendingData; }
		ReceivingData* receiving() { return m_pReceivingData; }
	};

	/**
	 * All the possible transfer types are defined below
	 */
	typedef Transfer<Message, Message> MsgTransfer;
	typedef Transfer<Segment, Message> SegMsgTransfer;
	typedef Transfer<Message, Segment> MsgSegTransfer;

} // namespace comm

#endif /* SRC_COMM_H_ */


