/*****************************************************************************
**																			**
**	 Name: 	Base Classes for a Communication libary							**
**																			**
*****************************************************************************/

#include "comm.h"

namespace comm {

	uint8_t MY_ID = 0x00;

	Message::Message()
	:
		m_srcID(0),
		m_destID(0),
		m_opCode(0),
		m_addr(0),
		m_ulData(0),
		m_dataCRC(0)
	{}
	Message::Message(uint8_t destID, uint8_t opCode, uint8_t addr, uint32_t ulData)
	{
		m_srcID = MY_ID;
		m_destID = destID;
		m_opCode = opCode;
		m_addr = addr;
		m_ulData =  ulData;
		m_dataCRC = Utils::calcCRC((uint8_t *)this, this->size());
	}
	bool Message::isForMe() const
	{
		return (m_destID == MY_ID);
	}
	bool Message::isOK() const
	{

		uint8_t byCrc = Utils::calcCRC((uint8_t *)this, this->size()-1);
		return(m_dataCRC == Utils::calcCRC((uint8_t *)this, this->size()-1));
	}
	bool Segment::isForMe() const
	{
		return (m_destID == MY_ID);
	}
	bool Segment::isOK() const
	{
		//return (m_headerCRC == utils::crc8((uint8_t*)(this), 0, DATA_OFFSET - 1) && (m_dataCRC == utils::crc8(m_aData, 0, DATA_SIZE)));
		//return (m_headerCRC == utils::crc8((uint8_t*)(this), 0, DATA_OFFSET - 1) && (m_dataCRC == utils::crc8(m_aData, 0, DATA_SIZE)));
		return (m_headerCRC == Utils::calcCRC((uint8_t*)(this), this->size()) && (m_dataCRC == Utils::calcCRC((uint8_t *)m_aData, DATA_SIZE)));

	}
    File::File()
    :	m_usNumber(0),
		m_usTotalNumberOfSegments(0),
		m_usSizeOfTheLastSegment(0),
		m_ulLength(0),
		m_usNextSegmentNumber(0),
		m_pData(0)
	{}

	File::File(uint8_t* pFileBuffer)
	:	m_usNumber(0),
		m_usTotalNumberOfSegments(0),
		m_usSizeOfTheLastSegment(0),
		m_ulLength(0),
		m_usNextSegmentNumber(0),
		m_pData(pFileBuffer)
	{}

	File::File(uint8_t destID, uint16_t usNumber, void* pData, uint32_t ulDataSize)
	:	m_destID(destID),
	    m_srcID(MY_ID),
	    m_usNumber(usNumber),
	    m_pData((uint8_t*)pData),
	    m_usNextSegmentNumber(0)
	{
		m_usTotalNumberOfSegments = ulDataSize / Segment::DATA_SIZE;
		m_usSizeOfTheLastSegment = ulDataSize % Segment::DATA_SIZE;
		m_ulLength = m_usTotalNumberOfSegments * Segment::DATA_SIZE;

	    if (m_usSizeOfTheLastSegment) // the last segment is not full
	    {
	        m_ulLength = m_ulLength + m_usSizeOfTheLastSegment;
	    	m_usTotalNumberOfSegments++;
	    }
	    else
	    	m_usSizeOfTheLastSegment = Segment::DATA_SIZE;
	}
    bool File::getNextSegment(Segment& seg)
	{
	    int i; //loop variable
	    if (m_usNextSegmentNumber < m_usTotalNumberOfSegments)
	    {
			seg.m_srcID = MY_ID;
			seg.m_destID = m_destID;
			seg.m_usFileNumber = m_usNumber; // file number
			seg.m_ulFileLength = m_ulLength; //file length in bytes
			seg.m_usNumber = m_usNextSegmentNumber; //number of this segment
			//seg.m_headerCRC = utils::crc8((uint8_t*)(&seg), 0, Segment::DATA_OFFSET - 1);

			seg.m_headerCRC = Utils::calcCRC((uint8_t*)(&seg), Segment::DATA_OFFSET - 1);



			if (m_usNextSegmentNumber == m_usTotalNumberOfSegments - 1) //the last segment
			{
				for (i = 0; i < m_usSizeOfTheLastSegment; i++)
				{
			    	seg.m_aData[i] = ((uint8_t*)m_pData)[Segment::DATA_SIZE * m_usNextSegmentNumber + i];
				}
				// fill the rest of the data with zeros
				for (i = m_usSizeOfTheLastSegment; i < Segment::DATA_SIZE; i++)
				{
			    	seg.m_aData[i] = 0x00;
				}
			}
			else // not the last segment
			{
			   	for (i = 0; i < Segment::DATA_SIZE; i++)
				{
			    	seg.m_aData[i] = ((uint8_t*)m_pData)[Segment::DATA_SIZE * m_usNextSegmentNumber + i];
				}
			}
			//seg.m_dataCRC = utils::crc8(seg.m_aData, 0, Segment::DATA_SIZE);

			seg.m_dataCRC = Utils::calcCRC((uint8_t *)seg.m_aData,Segment::DATA_SIZE);

			m_usNextSegmentNumber++;
			return true;
		}
		else return false;
	}
	uint32_t File::getDataSize() const
	{
		return (m_usTotalNumberOfSegments - 1) * Segment::DATA_SIZE + m_usSizeOfTheLastSegment;
	}
	bool File::append(Segment* seg)
	{
	    if (seg->isOK() && seg->m_usNumber == m_usNextSegmentNumber)
	    {
			if (seg->m_usNumber == 0) // the first segment
			{
				m_srcID = seg->m_srcID;
			    m_usNumber = seg->m_usFileNumber;
			    m_usTotalNumberOfSegments = seg->m_ulFileLength / Segment::DATA_SIZE;
				m_usSizeOfTheLastSegment = seg->m_ulFileLength % Segment::DATA_SIZE;
				if (m_usSizeOfTheLastSegment) // the last segment is not full
			    {
			    	m_usTotalNumberOfSegments++;
			   	}
			   	else
					m_usSizeOfTheLastSegment = Segment::DATA_SIZE;
			}
			uint32_t ulOffset = seg->m_usNumber * comm::Segment::DATA_SIZE;
	    	for ( int i(0); i < comm::Segment::DATA_SIZE; i ++)
	    	{
	    	        m_pData[i + ulOffset] = seg->m_aData[i];
	    	}
			m_usNextSegmentNumber++;
			return true;
	    }
		else
	    	return false;
	}
	bool File::segmentIsFirst() const
	{
		return (m_usNextSegmentNumber - 1 == 0);
	}
	bool File::segmentIsLast() const
	{
		if (m_usNextSegmentNumber - 1 == m_usTotalNumberOfSegments -1) // the last segment
			return true;
		else
			return false;
	}
}



