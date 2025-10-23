/*****************************************************************************
**																			**
**	 Name: 	Base Classes for a Communication libary							**
**																			**
*****************************************************************************/

#include <comm.hpp>

namespace comm {

	//extern uint8_t MY_ID;// = 0x00;

	Message::Message()
	:
		m_Header(0),
		m_srcID(0),
		m_destID(0),
		m_opCode(0),
		m_addr(0),
		m_length(0),
		m_payload(0),
		m_dataCRC(0),
		m_Footer(0)
	{}

	Message::Message(uint8_t destID, uint8_t opCode, uint8_t addr, const uint8_t* data, uint8_t length)
	:
		m_Header(0xAA),
		m_srcID(MY_ID),
		m_destID(destID),
		m_opCode(opCode),
		m_addr(addr),
		m_length(length),
		m_payload(data, data + length),
		m_dataCRC(0),
		m_Footer(0x55)
	{
		uint8_t buffer[256];
		this->rawBuffer(buffer); // populate full message layout
		m_dataCRC = calcCRC(buffer + 1, 5 + m_length);  // correct CRC range
	}


	bool Message::isForMe(uint8_t MY_ID) const {
		return (m_destID == MY_ID);
	}

	bool Message::isOK() const {
		uint8_t buffer[256];
		this->rawBuffer(buffer);
		uint8_t crc = calcCRC(buffer + 1, 5 + m_length);  // match encoding logic
		return m_dataCRC == crc;
	}

	size_t Message::size() const {
		return sizeof(m_srcID) + sizeof(m_destID) + sizeof(m_opCode) + sizeof(m_addr) +
		       sizeof(m_length) + m_length + sizeof(m_dataCRC);
	}

	const uint8_t* Message::data() const {
		return m_payload.data();
	}

	uint8_t Message::getOpCode() const {
	    return m_opCode;
	}


	uint8_t* Message::rawBuffer(uint8_t* buffer) const {
		uint8_t* p = buffer;
		*p++ = m_Header;
		*p++ = m_srcID;
		*p++ = m_destID;
		*p++ = m_opCode;
		*p++ = m_addr;
		*p++ = m_length;
		memcpy(p, m_payload.data(), m_length); p += m_length;
		*p++ = m_dataCRC;
		*p++ = m_Footer;
		return buffer;
	}

	uint8_t Message::calcCRC(const uint8_t* data, size_t length) const {
		uint8_t crc = 0;
		for (size_t i = 0; i < length; ++i) {
			crc ^= data[i];  // Simple XOR-based CRC
		}
		return crc;
	}

}
