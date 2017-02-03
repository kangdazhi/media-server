#include <arpa/inet.h>
#include <stdlib.h>
#include "log.h"
#include "rtp.h"
#include "bitstream.h"

RTPLostPackets::RTPLostPackets(WORD num)
{
	//Store number of packets
	size = num;
	//Create buffer
	packets = (QWORD*) malloc(num*sizeof(QWORD));
	//Set to 0
	memset(packets,0,size*sizeof(QWORD));
	//No first packet
	first = 0;
	//None yet
	len = 0;
}

void RTPLostPackets::Reset()
{
	//Set to 0
	memset(packets,0,size*sizeof(QWORD));
	//No first packet
	first = 0;
	//None yet
	len = 0;
}

RTPLostPackets::~RTPLostPackets()
{
	free(packets);
}

WORD RTPLostPackets::AddPacket(const RTPTimedPacket *packet)
{
	int lost = 0;
	
	//Get the packet number
	DWORD extSeq = packet->GetExtSeqNum();
	
	//Check if is befor first
	if (extSeq<first)
		//Exit, very old packet
		return 0;

	//If we are first
	if (!first)
		//Set to us
		first = extSeq;
	       
	//Get our position
	WORD pos = extSeq-first;
	
	//Check if we are still in window
	if (pos+1>size) 
	{
		//How much do we need to remove?
		int n = pos+1-size;
		//Check if we have to much to remove
		if (n>size)
			//cap it
			n = size;
		//Move
		memmove(packets,packets+n,(size-n)*sizeof(QWORD));
		//Fill with 0 the new ones
		memset(packets+(size-n),0,n*sizeof(QWORD));
		//Set first
		first = extSeq-size+1;
		//Full
		len = size;
		//We are last
		pos = size-1;
	} 
	
	//Check if it is last
	if (len-1<=pos)
	{
		//lock until we find a non lost and increase counter in the meanwhile
		for (int i=pos-1;i>=0 && !packets[i];--i)
			//Lost
			lost++;
		//Update last
		len = pos+1;
	}
	
	//Set
	packets[pos] = packet->GetTime();
	
	//Return lost ones
	return lost;
}


std::list<RTCPRTPFeedback::NACKField*> RTPLostPackets::GetNacks()
{
	std::list<RTCPRTPFeedback::NACKField*> nacks;
	WORD lost = 0;
	BYTE mask[2];
	BitWritter w(mask,2);
	int n = 0;
	
	//Iterate packets
	for(WORD i=0;i<len;i++)
	{
		//Are we in a lost count?
		if (lost)
		{
			//It was lost?
			w.Put(1,packets[i]==0);
			//Increase mask len
			n++;
			//If we are enought
			if (n==16)
			{
				//Flush
				w.Flush();
				//Add new NACK field to list
				nacks.push_back(new RTCPRTPFeedback::NACKField(lost,mask));
				//Empty for next
				w.Reset();
				//Reset counters
				n = 0;
				lost = 0;
			}
		}
		//Is this the first one lost
		else if (!packets[i]) {
			//This is the first one
			lost = first+i;
		}
		
	}
	
	//Are we in a lost count?
	if (lost)
	{
		//Fill reset with 0
		w.Put(16-n,0);
		//Flush
		w.Flush();
		//Add new NACK field to list
		nacks.push_back(new RTCPRTPFeedback::NACKField(lost,mask));
	}
	
	
	return nacks;
}

void  RTPLostPackets::Dump()
{
	Debug("[RTPLostPackets size=%d first=%d len=%d]\n",size,first,len);
	for(int i=0;i<len;i++)
		Debug("[%.3d,%.8d]\n",i,packets[i]);
	Debug("[/RTPLostPackets]\n");
}
