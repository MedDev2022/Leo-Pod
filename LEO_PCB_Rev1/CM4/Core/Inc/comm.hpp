
/*****************************************************************************
**																			**
**	 Name: 	Base Classes for a Communication libary							**
**																			**
*****************************************************************************/

#ifndef SRC_COMM_H_
#define SRC_COMM_H_

#include <cstdint>
#include <vector>
#include <cstring> // for memcpy

namespace comm {
	extern uint8_t MY_ID;

	/**
	 * Calculate a CRC for a given array, starting from an offset
	 *
	 * @param	aData			a data array
	 * @param	ulOffset		an offset
	 * @param	ulLength		a length of a data to calculate CRC
	 * @return					none
	 */
	uint8_t CRC8( const uint8_t aData[], uint32_t ulOffset, uint32_t ulLength );

	/**
	 * A base class for a basic command communication unit.
	 */
	class Message {
	public:
		Message();
		Message(uint8_t destID, uint8_t opCode, uint8_t addr, const uint8_t* data, uint8_t length);

		bool isForMe(uint8_t MY_ID) const;
		bool isOK() const;

		uint8_t getOpCode() const;  // <-- new accessor

		size_t size() const;
		const uint8_t* data() const;
		uint8_t* rawBuffer(uint8_t* buffer) const;
		uint8_t calcCRC(const uint8_t* data, size_t length) const;

		static constexpr size_t HEADER_SIZE = 6;//sizeof(m_Header)+sizeof(m_srcID) + sizeof(m_destID) +
		                                      //sizeof(m_opCode) + sizeof(m_addr) +
		                                     // sizeof(m_length);
		uint8_t getDestID() const { return m_destID; }
		uint8_t getLength() const { return m_length; }

	    std::vector<uint8_t>& payload() { return m_payload; }
	    const std::vector<uint8_t>& payload() const { return m_payload; }

	    void setCRC(uint8_t crc) { m_dataCRC = crc; }
	    void setFooter(uint8_t footer) { m_Footer = footer; }

	//protected:
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
	 * A base class for a basic file communication unit.
	 */
    class Segment
    {
    	friend class File;
        public:
        	static const uint16_t OVERHEAD_SIZE = 12;
			static const uint16_t DATA_OFFSET	= 11;
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
			 * Get a file number to witch the segment belongs
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
			uint8_t		m_srcID;
			uint8_t		m_destID;
			uint16_t	m_usFileNumber;
			uint32_t	m_ulFileLength;
			uint16_t	m_usNumber;
			uint8_t		m_headerCRC;
			uint8_t		m_aData[DATA_SIZE];
			uint8_t		m_dataCRC;
			uint16_t	m_length;
	};
	/**
	 * A class to manage file tranfer
	 */
	class File
	{
		public:
			/**
			 * A constructor that creates an empty file object.
			 *
			 * @return					none
			 */
			File();
			/**
			 * A constructor that creates a file object to receive file to.
			 *
			 * @param	pFileBuffer		a buffer to store the file to, should have the right size
			 * @return					none
			 */
			File(uint8_t* pFileBuffer);
			/**
			 * A constructor that creates a file object to be sent
			 *
			 * @param	destID			a destID
			 * @param	usNumber		a file number
			 * @param	pData			a data location for the file, read only
			 * @param	ulDataSize		a size of the data to be sent
			 * @return					none
			 */
			File(uint8_t destID, uint16_t usNumber, void* pData, uint32_t ulDataSize);
			/**
			 * Prepare the next segment to be sent, fill all the fields with data
			 *
			 * @param	seg				a segment to be filled
			 * @return					file number
			 */
			bool getNextSegment(Segment& seg);
			/**
			 * Get a destination ID of the file
			 *
			 * @return					destination ID
			 */
			uint8_t getDestID() const { return m_destID; }
			/**
			 * Get a source ID of the file
			 *
			 * @return					source ID
			 */
			uint8_t getSrcID() const { return m_srcID; }
			/**
			 * Get a size of the data in this file
			 *
			 * @return					data size
			 */
			uint32_t getDataSize() const;
			/**
			 * Get a file number
			 *
			 * @return					file number
			 */
			uint16_t getNumber() const { return m_usNumber; }
			/**
			 * Get a data pointer
			 *
			 * @return					data pointer
			 */
			uint8_t* getData() { return m_pData; }
			/**
			 * Get the curent segment number
			 *
			 * @return					segment number
			 */
			uint16_t getSegmentNumber() { return (m_usNextSegmentNumber - 1); }
			/**
			 * Copy a received segment into the file
			 *
			 * @return					none
			 */
			bool append(Segment* seg);
			/**
			 * Check if we are at the last segment
			 *
			 * @return					true: we are
			 */
			bool segmentIsLast() const;
			/**
			 * Check if we are at the first segment
			 *
			 * @return					true: we are
			 */
			bool segmentIsFirst() const;
		private:
			uint8_t	m_destID;
			uint8_t	m_srcID;
			uint16_t	m_usNumber;
			uint16_t	m_usTotalNumberOfSegments;
			uint16_t	m_usSizeOfTheLastSegment;
			uint32_t	m_ulLength;
			uint16_t	m_usNextSegmentNumber;
			uint8_t*	m_pData;
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
		/**
		 * A constructor that creates an empty transfer object
		 *
		 * @return					none
		 */
    	Transfer()
    	: m_pSendingData(0), m_pReceivingData(0){}
		/**
		 * A constructor that creates a transfer object using given data
		 *
		 * @param	pSendingData	a pointer to the data unit to be sent
		 * @param	pReceivingData	a pointer to the data unit to be received
		 * @return					none
		 */
    	Transfer(SendingData* pSendingData, ReceivingData* pReceivingData)
        : m_pSendingData(pSendingData), m_pReceivingData(pReceivingData) {}

         /**
		 * A template method to start the transfer
		 * if there is a data to be sent, sends it
		 * if there is a data to receive, waits for it
		 *
		 * @param	port			a port used for transfer
		 * @return					none
		 */
        template <typename Port>
        void start(Port& port) {
			if (m_pSendingData)
				port->send(m_pSendingData, m_pSendingData->size());
			if (m_pReceivingData)
				port->receive(m_pReceivingData, m_pReceivingData->size());
        }
        /**
		 * A template method that check if we recieved any data
		 * it returns true if the data was received and is ment for us
		 *
		 * @param	port			a port used for transfer
		 * @return					none
		 */
        template <typename Port>
		bool run(Port& port) {
			if (m_pReceivingData) {
		    	if (port->received()) {
		    		if(m_pReceivingData->isForMe() && m_pReceivingData->isOK())
						return true;
					else
						port->receive();
		    	}
			}
			return false;
		}
		/**
		 * Get a pointer to the data unit we are sending from
		 *
		 * @return					a pointer to the data unit
		 */
    	SendingData* sending() { return m_pSendingData; }
		/**
		 * Get a pointer to the data unit we are receiving to
		 *
		 * @return					a pointer to the data unit
		 */
    	ReceivingData* receiving() { return m_pReceivingData; }

    };

    /**
	 * All the possible transfer types are defined below
	 */
    typedef Transfer<Message, Message> MsgTransfer;
	typedef Transfer<Segment, Message> SegMsgTransfer;
    typedef	Transfer<Message, Segment> MsgSegTransfer;
}


#endif /* SRC_COMM_H_ */


