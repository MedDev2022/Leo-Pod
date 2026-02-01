/*****************************************************************************
**																			**
**	 Name: 	Base Classes for a Communication libary							**
**																			**
*****************************************************************************/

#include <comm.hpp>
#include <algorithm>

namespace comm {

// ========================================================================
// Device Name Lookup
// ========================================================================

const char* getDeviceName(DeviceID id) {
	switch (id) {
		case DeviceID::HOST:   return "HOST";
		case DeviceID::LEOPOD: return "LEOPOD";
		case DeviceID::DAYCAM: return "DAYCAM";
		case DeviceID::IRAY:   return "IRAY";
		case DeviceID::RPLENS: return "RPLENS";
		case DeviceID::LRF:    return "LRF";
		default:               return "UNKNOWN";
	}
}

const char* getDeviceName(uint8_t id) {
	return getDeviceName(static_cast<DeviceID>(id));
}

// ========================================================================
// CRC Calculation
// ========================================================================

uint8_t CRC8(const uint8_t aData[], uint32_t ulOffset, uint32_t ulLength) {
	uint8_t crc = 0x00;
	for (uint32_t i = 0; i < ulLength; ++i) {
		crc ^= aData[ulOffset + i];
	}
	return crc;
}

// ========================================================================
// Message Implementation
// ========================================================================

Message::Message()
	: m_Header(HEADER_BYTE)
	, m_srcID(0)
	, m_destID(0)
	, m_opCode(0)
	, m_addr(0)
	, m_length(0)
	, m_payload()
	, m_dataCRC(0)
	, m_Footer(FOOTER_BYTE)
{}

Message::Message(uint8_t destID, uint8_t opCode, uint8_t addr, const uint8_t* data, uint8_t length)
	: m_Header(HEADER_BYTE)
	, m_srcID(MY_ID)
	, m_destID(destID)
	, m_opCode(opCode)
	, m_addr(addr)
	, m_length(length)
	, m_payload(data, data + length)
	, m_dataCRC(0)
	, m_Footer(FOOTER_BYTE)
{
	updateCRC();
}

Message::Message(DeviceID source, DeviceID destination, uint8_t opCode)
	: m_Header(HEADER_BYTE)
	, m_srcID(static_cast<uint8_t>(source))
	, m_destID(static_cast<uint8_t>(destination))
	, m_opCode(opCode)
	, m_addr(0)
	, m_length(0)
	, m_payload()
	, m_dataCRC(0)
	, m_Footer(FOOTER_BYTE)
{
	updateCRC();
}

Message::Message(DeviceID source, DeviceID destination, uint8_t opCode, uint8_t addr,
                 const uint8_t* data, uint8_t length)
	: m_Header(HEADER_BYTE)
	, m_srcID(static_cast<uint8_t>(source))
	, m_destID(static_cast<uint8_t>(destination))
	, m_opCode(opCode)
	, m_addr(addr)
	, m_length(length)
	, m_payload(data, data + length)
	, m_dataCRC(0)
	, m_Footer(FOOTER_BYTE)
{
	updateCRC();
}

// ========================================================================
// Validation Methods
// ========================================================================

bool Message::isForMe(uint8_t MY_ID) const {
	return (m_destID == MY_ID);
}

bool Message::isForMe(DeviceID myID) const {
	return (m_destID == static_cast<uint8_t>(myID));
}

bool Message::isOK() const {
	// Verify header and footer
	if (m_Header != HEADER_BYTE || m_Footer != FOOTER_BYTE) {
		return false;
	}

	// Verify payload length matches
	if (m_length != m_payload.size()) {
		return false;
	}

	// Verify CRC
	uint8_t buffer[256];
	rawBuffer(buffer);
	uint8_t calculatedCRC = calcCRC(buffer + 1, 5 + m_length);

	return (m_dataCRC == calculatedCRC);
}

bool Message::isValid() const {
	return (m_Header == HEADER_BYTE &&
	        m_Footer == FOOTER_BYTE &&
	        m_length <= MAX_PAYLOAD_SIZE &&
	        m_length == m_payload.size());
}

// ========================================================================
// Setters
// ========================================================================

void Message::setPayload(const uint8_t* data, uint8_t length) {
	if (length > MAX_PAYLOAD_SIZE) {
		length = MAX_PAYLOAD_SIZE;
	}
	m_payload.assign(data, data + length);
	m_length = length;
}

void Message::setPayload(const std::vector<uint8_t>& data) {
	if (data.size() > MAX_PAYLOAD_SIZE) {
		m_payload.assign(data.begin(), data.begin() + MAX_PAYLOAD_SIZE);
		m_length = MAX_PAYLOAD_SIZE;
	} else {
		m_payload = data;
		m_length = static_cast<uint8_t>(data.size());
	}
}

// ========================================================================
	// Message Building
	// ========================================================================

	size_t Message::size() const {
		// Size without header and footer
		return 5 + m_length + 1;  // SrcID + DestID + OpCode + Addr + Length + Payload + CRC
	}

	size_t Message::totalSize() const {
		// Total size including header and footer
		return 8 + m_length;  // Header + fields + payload + CRC + Footer
	}

	uint8_t* Message::rawBuffer(uint8_t* buffer) const {
		uint8_t* p = buffer;
		*p++ = m_Header;
		*p++ = m_srcID;
		*p++ = m_destID;
		*p++ = m_opCode;
		*p++ = m_addr;
		*p++ = m_length;
		if (m_length > 0) {
			memcpy(p, m_payload.data(), m_length);
			p += m_length;
		}
		*p++ = m_dataCRC;
		*p++ = m_Footer;
		return buffer;
	}

	std::vector<uint8_t> Message::build() const {
		std::vector<uint8_t> buffer;
		buffer.reserve(totalSize());

		buffer.push_back(m_Header);
		buffer.push_back(m_srcID);
		buffer.push_back(m_destID);
		buffer.push_back(m_opCode);
		buffer.push_back(m_addr);
		buffer.push_back(m_length);

		if (m_length > 0) {
			buffer.insert(buffer.end(), m_payload.begin(), m_payload.end());
		}

		buffer.push_back(m_dataCRC);
		buffer.push_back(m_Footer);

		return buffer;
	}

	void Message::updateCRC() {
		uint8_t buffer[256];
		rawBuffer(buffer);
		m_dataCRC = calcCRC(buffer + 1, 5 + m_length);
	}

	// ========================================================================
	// Message Parsing
	// ========================================================================

	uint16_t Message::parse(const uint8_t* buffer, size_t length) {
		// Minimum message: Header + SrcID + DestID + OpCode + Addr + Length + CRC + Footer = 8
		if (length < MIN_MESSAGE_SIZE) {
			return false;
		}

		// Verify header and footer
		if (buffer[0] != HEADER_BYTE || buffer[length - 1] != FOOTER_BYTE) {
			return false;
		}

		// Parse fields
		m_Header = buffer[0];
		m_srcID = buffer[1];
		m_destID = buffer[2];
		m_opCode = buffer[3];
		m_addr = buffer[4];
		m_length = buffer[5];

		// Verify payload length
		if (m_length > MAX_PAYLOAD_SIZE) {
			return false;
		}

		// Check total length consistency
		size_t expectedLen = 8 + m_length;  // Header + 5 fields + payload + CRC + Footer
		if (length != expectedLen) {
			return false;
		}

		// Extract payload
		m_payload.clear();
		if (m_length > 0) {
			m_payload.assign(buffer + 6, buffer + 6 + m_length);
		}

		// Extract CRC and footer
		m_dataCRC = buffer[6 + m_length];
		m_Footer = buffer[6 + m_length + 1];

		// Verify CRC
		uint8_t calculatedCRC = calcCRC(buffer + 1, 5 + m_length);

		return (m_dataCRC == calculatedCRC);
	}

//	uint16_t Message::parse(const uint8_t* data, size_t maxLength) {
//	    // Check minimum size for header
//	    if (maxLength < HEADER_SIZE) return 0;
//
//	    // Check for start byte
//	    if (data[0] != START_BYTE) return 0;
//
//	    // Extract payload length from header
//	    uint16_t payloadLen = data[HEADER_SIZE - 1];
//
//	    // Calculate total message size
//	    uint16_t totalSize = HEADER_SIZE + payloadLen + CRC_SIZE + FOOTER_SIZE;
//
//	    // Not enough data for complete message?
//	    if (maxLength < totalSize) return 0;
//
//		m_Header = data[0];
//		m_srcID = data[1];
//		m_destID = data[2];
//		m_opCode = data[3];
//		m_addr = data[4];
//		m_length = data[5];
//		// Extract CRC and footer
//		m_dataCRC = data[HEADER_SIZE + m_length];
//		m_Footer = data[HEADER_SIZE + m_length + 1];
//
//
//		// Extract payload
//		m_payload.clear();
//		if (m_length > 0) {
//			m_payload.assign(data + 6, data + 6 + m_length);
//		}
//
//
//		// Verify CRC
//		uint8_t calculatedCRC = calcCRC(data + 1, 5 + m_length);
//
//		if (m_dataCRC == calculatedCRC)
//			return totalSize;
//		else
//			return 0;
//
//	}

	bool Message::verify(const uint8_t* buffer, size_t length) {
		// Quick verification without parsing
		if (length < MIN_MESSAGE_SIZE) {
			return false;
		}

		if (buffer[0] != HEADER_BYTE || buffer[length - 1] != FOOTER_BYTE) {
			return false;
		}

		uint8_t payloadLength = buffer[5];

		if (payloadLength > MAX_PAYLOAD_SIZE) {
			return false;
		}

		size_t expectedLen = 8 + payloadLength;
		if (length != expectedLen) {
			return false;
		}

		// Verify CRC
		uint8_t receivedCRC = buffer[6 + payloadLength];
		uint8_t calculatedCRC = calculateCRC(buffer, length);

		return (receivedCRC == calculatedCRC);
	}

	size_t Message::expectedLength(const uint8_t* buffer, size_t currentLength) {
		// Need at least 6 bytes to read length field
		if (currentLength < 6) {
			return 0;  // Not enough data yet
		}

		uint8_t payloadLength = buffer[5];

		if (payloadLength > MAX_PAYLOAD_SIZE) {
			return 0;  // Invalid
		}

		return 8 + payloadLength;  // Header + fields + payload + CRC + Footer
	}

	// ========================================================================
	// CRC Calculation
	// ========================================================================

	uint8_t Message::calcCRC(const uint8_t* data, size_t length) const {
		uint8_t crc = 0x00;
		for (size_t i = 0; i < length; ++i) {
			crc ^= data[i];
		}
		return crc;
	}

	uint8_t Message::calculateCRC(const uint8_t* buffer, size_t length) {
		// CRC is XOR of bytes from SrcID to end of payload
		// Excludes: Header (index 0), CRC itself, Footer
		uint8_t crc = 0x00;

		// Get payload length
		uint8_t payloadLength = buffer[5];

		// Calculate CRC from index 1 (SrcID) to 5+payloadLength (end of payload)
		for (size_t i = 1; i <= 5 + payloadLength; ++i) {
			crc ^= buffer[i];
		}

		return crc;
	}

	// ========================================================================
	// MessageBuilder Implementation
	// ========================================================================

	MessageBuilder::MessageBuilder(DeviceID source, DeviceID destination)
		: message_(source, destination, 0)
	{}

	MessageBuilder::MessageBuilder(uint8_t source, uint8_t destination)
		: message_(static_cast<DeviceID>(source), static_cast<DeviceID>(destination), 0)
	{}

	MessageBuilder& MessageBuilder::opCode(uint8_t code) {
		message_.setOpCode(code);
		return *this;
	}

	MessageBuilder& MessageBuilder::address(uint8_t addr) {
		message_.setAddr(addr);
		return *this;
	}

	MessageBuilder& MessageBuilder::payload(const std::vector<uint8_t>& data) {
		message_.setPayload(data);
		return *this;
	}

	MessageBuilder& MessageBuilder::payload(const uint8_t* data, uint8_t length) {
		message_.setPayload(data, length);
		return *this;
	}

	std::vector<uint8_t> MessageBuilder::build() {
		message_.updateCRC();
		return message_.build();
	}

	Message MessageBuilder::buildMessage() {
		message_.updateCRC();
		return message_;
	}

} // namespace comm
