/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011-2012 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Manuel Requena <manuel.requena@cttc.es>
 */

#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/socket.h"
#include "ns3/lte-pdcp.h"
#include "ns3/lte-pdcp-header.h"
#include "ns3/lte-pdcp-sap.h"
#include "ns3/lte-pdcp-tag.h"
#include "ns3/lwalwip-header.h"
#include "ns3/lwa-tag.h"
#include "ns3/lwip-tag.h"
#include "ns3/pdcp-lcid.h"


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LtePdcp");

/// LtePdcpSpecificLteRlcSapUser class
class LtePdcpSpecificLteRlcSapUser : public LteRlcSapUser
{
public:
  /**
   * Constructor
   *
   * \param pdcp PDCP
   */
  LtePdcpSpecificLteRlcSapUser (LtePdcp* pdcp);

  // Interface provided to lower RLC entity (implemented from LteRlcSapUser)
  virtual void ReceivePdcpPdu (Ptr<Packet> p);

private:
  LtePdcpSpecificLteRlcSapUser ();
  LtePdcp* m_pdcp; ///< the PDCP
};

LtePdcpSpecificLteRlcSapUser::LtePdcpSpecificLteRlcSapUser (LtePdcp* pdcp)
  : m_pdcp (pdcp)
{
}

LtePdcpSpecificLteRlcSapUser::LtePdcpSpecificLteRlcSapUser ()
{
}

void
LtePdcpSpecificLteRlcSapUser::ReceivePdcpPdu (Ptr<Packet> p)
{
  m_pdcp->DoReceivePdu (p);
}

///////////////////////////////////////

NS_OBJECT_ENSURE_REGISTERED (LtePdcp);

LtePdcp::LtePdcp ()
  : m_pdcpSapUser (0),
    m_rlcSapProvider (0),
    m_rnti (0),
    m_lcid (0),
	pdcp_decisionlwa(0),
	pdcp_decisionlwip(0),
    m_txSequenceNumber (0),
    m_rxSequenceNumber (0),
	ueReordering (false)
{
  NS_LOG_FUNCTION (this);
  m_pdcpSapProvider = new LtePdcpSpecificLtePdcpSapProvider<LtePdcp> (this);
  m_rlcSapUser = new LtePdcpSpecificLteRlcSapUser (this);
}

LtePdcp::~LtePdcp ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
LtePdcp::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::LtePdcp")
    .SetParent<Object> ()
	.AddConstructor<LtePdcp> ()
    .SetGroupName("Lte")
    .AddTraceSource ("TxPDU",
                     "PDU transmission notified to the RLC.",
                     MakeTraceSourceAccessor (&LtePdcp::m_txPdu),
                     "ns3::LtePdcp::PduTxTracedCallback")
    .AddTraceSource ("RxPDU",
                     "PDU received.",
                     MakeTraceSourceAccessor (&LtePdcp::m_rxPdu),
                     "ns3::LtePdcp::PduRxTracedCallback")
    .AddTraceSource ("TxPDUtrace",//modifications for trace
                     "PDU to be transmit.",//modifications for trace
                     MakeTraceSourceAccessor (&LtePdcp::m_pdcptxtrace),//modifications for trace
                     "ns3::Packet::TracedCallback")//modifications for trace
    .AddAttribute ("PDCPDecLwa",
                     "PDCP LWA or LWIP decision variable",
                     UintegerValue (0),
                     MakeUintegerAccessor (&LtePdcp::pdcp_decisionlwa),
                     MakeUintegerChecker<uint32_t>())
    .AddAttribute ("PDCPDecLwip",
                     "PDCP LWIP decision variable",
                     UintegerValue (0),
                     MakeUintegerAccessor (&LtePdcp::pdcp_decisionlwip),
                     MakeUintegerChecker<uint32_t>())
	 .AddAttribute ("ReorderingTimer",
						"Value of the t-Reordering timer (See section 7.3 of 3GPP TS 36.323)",
						TimeValue (MilliSeconds (100)),
						MakeTimeAccessor (&LtePdcp::m_reorderingTimerValue),
						MakeTimeChecker ())
    ;
  return tid;
}

void
LtePdcp::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  delete (m_pdcpSapProvider);
  delete (m_rlcSapUser);
  m_reorderingTimer.Cancel ();
}


void
LtePdcp::SetRnti (uint16_t rnti)
{
  NS_LOG_FUNCTION (this << (uint32_t) rnti);
  m_rnti = rnti;
}

void
LtePdcp::SetLcId (uint8_t lcId)
{
  NS_LOG_FUNCTION (this << (uint32_t) lcId);
  m_lcid = lcId;
}

void
LtePdcp::SetUeReordering (bool ueReorderingIn)
{
  NS_LOG_FUNCTION (this << ueReorderingIn);
  ueReordering = ueReorderingIn;
}

bool
LtePdcp::GetUeReordering ()
{
  NS_LOG_FUNCTION (this << (bool) ueReordering);
  return ueReordering;
}
void
LtePdcp::SetLtePdcpSapUser (LtePdcpSapUser * s)
{
  NS_LOG_FUNCTION (this << s);
  m_pdcpSapUser = s;
}

LtePdcpSapProvider*
LtePdcp::GetLtePdcpSapProvider ()
{
  NS_LOG_FUNCTION (this);
  return m_pdcpSapProvider;
}

void
LtePdcp::SetLteRlcSapProvider (LteRlcSapProvider * s)
{
  NS_LOG_FUNCTION (this << s);
  m_rlcSapProvider = s;
}

LteRlcSapUser*
LtePdcp::GetLteRlcSapUser ()
{
  NS_LOG_FUNCTION (this);
  return m_rlcSapUser;
}

LtePdcp::Status 
LtePdcp::GetStatus ()
{
  Status s;
  s.txSn = m_txSequenceNumber;
  s.rxSn = m_rxSequenceNumber;
  return s;
}

void 
LtePdcp::SetStatus (Status s)
{
  m_txSequenceNumber = s.txSn;
  m_rxSequenceNumber = s.rxSn;
}

void
LtePdcp::SetLwaRxSocket (Ptr<Socket> socket)
{
	m_staRxSocket = socket;
	// m_staRxSocket->SetRecvCallback( MakeNullCallback<void, Ptr<Socket>>());
	m_staRxSocket->SetRecvCallback( MakeCallback(&LtePdcp::ReceiveLwaPacket, this));
}
/*
uint32_t
LtePdcp::GetPdcpDecisionLwa ()
{
  return pdcp_decisionlwa;
}

void
LtePdcp::SetPdcpDecisionLwa (uint32_t lwa)
{
  pdcp_decisionlwa = lwa;
}

uint32_t
LtePdcp::GetPdcpDecisionLwip ()
{
  return pdcp_decisionlwip;
}

void
LtePdcp::SetPdcpDecisionLwip (uint32_t lwip)
{
  pdcp_decisionlwip = lwip;
}
*/
////////////////////////////////////////

void
LtePdcp::DoTransmitPdcpSdu (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << m_rnti << (uint32_t) m_lcid << p->GetSize ());

  NS_LOG_DEBUG("Node send PDCP data for lcid=" << (uint32_t) m_lcid << ", m_rnti=" <<  m_rnti << ", packet size=" << p->GetSize ());

  LtePdcpHeader pdcpHeader;
  pdcpHeader.SetSequenceNumber (m_txSequenceNumber);

  m_txSequenceNumber++;
  if (m_txSequenceNumber > m_maxPdcpSn)
    {
      m_txSequenceNumber = 0;
    }

  pdcpHeader.SetDcBit (LtePdcpHeader::DATA_PDU);

  NS_LOG_LOGIC ("PDCP header: " << pdcpHeader);
  p->AddHeader (pdcpHeader);

  // Sender timestamp
  PdcpTag pdcpTag (Simulator::Now ());
  p->AddPacketTag (pdcpTag);
  m_txPdu (m_rnti, m_lcid, p->GetSize ());

  LteRlcSapProvider::TransmitPdcpPduParameters params;
  params.rnti = m_rnti;
  params.lcid = m_lcid;
  params.pdcpPdu = p;

  // No splitting at UE / Uplink
  if (!ueReordering) {
	  LwaTag LWATag;
	  LwipTag LWIPTag;
	  PdcpLcid lcidtag;

	  // switch between the lwa/lwip modes
	  if ((pdcp_decisionlwa>0)&&(m_lcid>=3)){
	      if(pdcp_decisionlwa==1){
	          // split packets between lwa und default lte link
	          if((m_packetCounter%2)==0){
	              m_rlcSapProvider->TransmitPdcpPdu (params);
	          }
	          if((m_packetCounter%2)==1){
	              LWATag.Set(pdcp_decisionlwa);
	              lcidtag.Set((uint32_t)m_lcid);
	              p->AddPacketTag (LWATag);
	              p->AddByteTag (lcidtag);
	              m_pdcptxtrace(params);
	          }
	          m_packetCounter++;
	      } else {
	          // all packets routed over lwa (all drb)
	          LWATag.Set(pdcp_decisionlwa);
	          lcidtag.Set((uint32_t)m_lcid);
	          p->AddPacketTag (LWATag);
	          p->AddByteTag (lcidtag);
	          m_pdcptxtrace(params);
	      }
	  } else {
	      // default lte link - used also always for srb (lcid 0/1/2)
	      m_rlcSapProvider->TransmitPdcpPdu (params);
	  }
  } else {
	  m_rlcSapProvider->TransmitPdcpPdu (params);
  }

}

void LtePdcp::ReceiveLwaPacket (Ptr<Socket> socket)
{
  Ptr<Packet> packet = socket->Recv ();
  //NS_LOG_DEBUG ("Packet received from LWAAP: " << packet->ToString() << " at " << socket->GetNode()->GetId());
  LwaLwipHeader lwaLwipHeader;
  packet->RemoveHeader(lwaLwipHeader);
  //if (lwaLwipHeader != null){
	  // NS_LOG_DEBUG ("Has LWA header with lcid: " << lwaLwipHeader.GetBearerId()+2);
	  //Ptr<LtePdcp> found = rlcMap.find(to_string(socket->GetNode()->GetId()))->second;
	  //NS_LOG_DEBUG ("Getting RLC Now.. "<< found->GetTypeId());
	  //found->GetLteRlcSapUser()->ReceivePdcpPdu(packet);
  //}
  DoReceivePdu(packet);
}

void
LtePdcp::DoReceivePdu (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << m_rnti << (uint32_t) m_lcid << p->GetSize ());
  // Receiver timestamp
  PdcpTag pdcpTag;
  Time delay;
  NS_ASSERT_MSG (p->PeekPacketTag (pdcpTag), "PdcpTag is missing");
  p->RemovePacketTag (pdcpTag);
  delay = Simulator::Now() - pdcpTag.GetSenderTimestamp ();
  m_rxPdu(m_rnti, m_lcid, p->GetSize (), delay.GetNanoSeconds ());

  LtePdcpHeader pdcpHeader;
  p->RemoveHeader (pdcpHeader);
  NS_LOG_LOGIC ("PDCP header: " << pdcpHeader);
  uint16_t currSeqNumber = pdcpHeader.GetSequenceNumber ();

  if (m_rxSequenceNumber > m_maxPdcpSn)
    {
	  NS_LOG_WARN ("resetting to 0");
      m_rxSequenceNumber = 0;
    }

  if ((pdcp_decisionlwa>0)&&(m_lcid>=3)){
	  std::string currentDevice = "ENB";
	  if (ueReordering) { NS_LOG_LOGIC ("PDCP Packet at UE: " << p->ToString()); currentDevice = "UE";}
	    else if(p->GetUid()) { NS_LOG_LOGIC ("PDCP Packet at ENB: " << p->ToString()); }
	  /*
	  m_rxSequenceNumber = pdcpHeader.GetSequenceNumber() + 1;
	  	  NS_LOG_LOGIC ("ENB sending up = " << currSeqNumber << ". Now expecting " << m_rxSequenceNumber);
	  	  LtePdcpSapUser::ReceivePdcpSduParameters params;
	  	  params.pdcpSdu = p;
	  	  params.rnti = m_rnti;
	  	  params.lcid = m_lcid;
	  	  m_pdcpSapUser->ReceivePdcpSdu (params);
	  	  */
	  	bool discard = false;
	  	uint32_t count;// = m_rxHfn << 12;
	  	// NS_LOG_LOGIC ("count: " << count);
	    if (((currSeqNumber - m_lastSubmittedRxSn) > m_reorderingWindow) ||
	  					(0 <= (m_lastSubmittedRxSn - currSeqNumber) && (m_lastSubmittedRxSn - currSeqNumber) < m_reorderingWindow)) {
				//TODO: LWA status
				discard = true;
			NS_LOG_LOGIC (currentDevice << " ignoring = " << currSeqNumber << " because expecting " << m_rxSequenceNumber);
		} else if((m_rxSequenceNumber - currSeqNumber) > m_reorderingWindow) {
			m_rxHfn += 1;
			NS_LOG_LOGIC ("Incrementing RxHFN");
			count = (m_rxHfn << 12) + currSeqNumber;
	    	m_rxSequenceNumber = currSeqNumber + 1;
	    } else if((currSeqNumber - m_rxSequenceNumber) >= m_reorderingWindow) {
	    	count = (m_rxHfn - 1 << 12) + currSeqNumber;
	    } else if (currSeqNumber >= m_rxSequenceNumber) {
	    	m_rxSequenceNumber = currSeqNumber + 1;
	    	count = (m_rxHfn << 12) + currSeqNumber;
	    	 if (m_rxSequenceNumber > m_maxPdcpSn)
	    	    {
	    		  NS_LOG_WARN ("resetting to 0");
	    	      m_rxSequenceNumber = 0;
	    	      m_rxHfn =+ 1;
	    	      NS_LOG_LOGIC ("Incrementing RxHFN");
	    	    }

	    } else if(currSeqNumber < m_rxSequenceNumber) {
	    	count = (m_rxHfn << 12) + currSeqNumber;
	    }

	     if (!discard) {
			NS_LOG_LOGIC (currentDevice << " storing = " << currSeqNumber << " because expecting " << m_lastSubmittedRxSn + 1);
			p->AddHeader(pdcpHeader);
	    	m_sdusBuffer.insert(std::pair<uint32_t, Ptr<Packet>>(count, p));
	    	if (currSeqNumber == (m_lastSubmittedRxSn + 1) || currSeqNumber == (m_lastSubmittedRxSn - m_maxPdcpSn)) {
				/*NS_LOG_LOGIC (currentDevice << " sending up = " << currSeqNumber << ". Now expecting " << m_rxSequenceNumber);
				LtePdcpSapUser::ReceivePdcpSduParameters params;
				params.pdcpSdu = p;
				params.rnti = m_rnti;
				params.lcid = m_lcid;
				m_pdcpSapUser->ReceivePdcpSdu (params);*/
				// m_rxSequenceNumber = currSeqNumber + 1;

				//while (!m_sdusBuffer.empty()) {
				  for (auto nh = m_sdusBuffer.find(count); nh != m_sdusBuffer.end() && !m_sdusBuffer.empty(); ) {
					  NS_LOG_LOGIC ("Buffer size: " << m_sdusBuffer.size());
					  NS_LOG_LOGIC (currentDevice << " sending up = " << nh->first << ". Now expecting " << m_lastSubmittedRxSn + 1);
					  LtePdcpSapUser::ReceivePdcpSduParameters paramsIn;
					  nh->second->RemoveHeader(pdcpHeader);
					  paramsIn.pdcpSdu = nh->second;
					  paramsIn.rnti = m_rnti;
					  paramsIn.lcid = m_lcid;
					  m_pdcpSapUser->ReceivePdcpSdu (paramsIn);
					  m_lastSubmittedRxSn = pdcpHeader.GetSequenceNumber();
					  m_sdusBuffer.erase(nh);
					  count += 1;
					  nh = m_sdusBuffer.find(count);
				  }
				  //if(m_lastSubmittedRxSn > (m_rxHfn << 12)) {
				 // m_lastSubmittedRxSn = m_lastSubmittedRxSn - count;
				//}
				//m_lastSubmittedRxSn = m_rxSequenceNumber - 1;

			}

	    	if ( m_reorderingTimer.IsRunning () )
			  {
				NS_LOG_LOGIC ("Reordering timer is running");

				if (  m_reorderingRxCount < m_lastSubmittedRxSn )
				  {
					NS_LOG_LOGIC ("Stop reordering timer");
					m_reorderingTimer.Cancel ();
				  }
			  }

			if ( ! m_reorderingTimer.IsRunning () )
				{
				  NS_LOG_LOGIC ("Reordering timer is not running");

				  if(!m_sdusBuffer.empty()) {
					  m_reorderingTimer = Simulator::Schedule (m_reorderingTimerValue, &LtePdcp::ExpireReorderingTimer ,this);
					  m_reorderingRxCount = (m_rxHfn << 12) + m_rxSequenceNumber;
				  }
				}
	    }

	    /*
	    if ( currSeqNumber < m_rxSequenceNumber ) {
	    	// ignore
	    	NS_LOG_LOGIC (currentDevice << " ignoring = " << currSeqNumber << " because expecting " << m_rxSequenceNumber);
	    } else if ( currSeqNumber > m_rxSequenceNumber ) {
			NS_LOG_LOGIC ("UE storing = " << currSeqNumber << " because expecting " << m_rxSequenceNumber);
			m_sdusBuffer.insert(std::pair<uint16_t, Ptr<Packet>>(currSeqNumber, p->Copy()));
			//m_rxSequenceNumber = pdcpHeader.GetSequenceNumber() + 1;
		} else {
			m_rxSequenceNumber = m_rxSequenceNumber + 1;
							 // NS_LOG_LOGIC ("UE restored and sent = " << paramsList.back().pdcpSdu->ToString());
		}
		*/


  } else {
	  m_rxSequenceNumber = pdcpHeader.GetSequenceNumber() + 1;
	  NS_LOG_LOGIC ("Normal LTE sending up = " << currSeqNumber << ". Now expecting " << m_rxSequenceNumber);
	  LtePdcpSapUser::ReceivePdcpSduParameters params;
	  params.pdcpSdu = p;
	  params.rnti = m_rnti;
	  params.lcid = m_lcid;
	  m_pdcpSapUser->ReceivePdcpSdu (params);
  }

  /*else if (ueReordering && pdcpHeader.GetDcBit()==LtePdcpHeader::DATA_PDU) {
    	if(m_rxSequenceNumber < pdcpHeader.GetSequenceNumber ()) {
    		// m_rxSequenceNumber = pdcpHeader.GetSequenceNumber () + 1;
    		oldPacket = p->Copy();
    		oldPacket -> AddHeader(pdcpHeader);
    		NS_LOG_LOGIC ("Out of order packet stored");
    	} else if (m_rxSequenceNumber == pdcpHeader.GetSequenceNumber () && oldPacket){
    		LtePdcpSapUser::ReceivePdcpSduParameters params;
    		  params.pdcpSdu = p;
    		  params.rnti = m_rnti;
    		  params.lcid = m_lcid;
    		  NS_LOG_WARN ("sending ahead... and re-ordering: " << ueReordering);
    		  m_pdcpSapUser->ReceivePdcpSdu (params);

    		  NS_LOG_LOGIC ("First packet with SN " << pdcpHeader.GetSequenceNumber () << " sent");
    		p = oldPacket;
    		LtePdcpHeader oldPdcpHeader;
    		p->RemoveHeader (oldPdcpHeader);
    		m_rxSequenceNumber = oldPdcpHeader.GetSequenceNumber() + 1;
    		NS_LOG_LOGIC ("Second packet with SN " << oldPdcpHeader.GetSequenceNumber () << " sent");

    		oldPacket = 0;
    	} else if(m_rxSequenceNumber > pdcpHeader.GetSequenceNumber ()) {
    		NS_LOG_LOGIC ("Dropped packet " << pdcpHeader.GetSequenceNumber());
    	} else {
        	m_rxSequenceNumber = pdcpHeader.GetSequenceNumber() + 1;
        }
    } else {
    	m_rxSequenceNumber = pdcpHeader.GetSequenceNumber() + 1;
    }*/

}


void
LtePdcp::ExpireReorderingTimer (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC ("Reordering timer has expired");
  LtePdcpHeader pdcpHeader;
  //uint32_t count = (m_rxHfn << 12) + m_lastSubmittedRxSn;
  //while (!m_sdusBuffer.empty()) {
  	  for (auto it = m_sdusBuffer.begin(); it != m_sdusBuffer.end() && !m_sdusBuffer.empty() && it->first <= m_reorderingRxCount; ++it) {
  	      NS_LOG_LOGIC (" sending up = " << it->first << ". Now expecting " << m_lastSubmittedRxSn + 1);
  		  LtePdcpSapUser::ReceivePdcpSduParameters paramsIn;
  		  it->second->RemoveHeader(pdcpHeader);
  		  paramsIn.pdcpSdu = it->second;
  		  paramsIn.rnti = m_rnti;
  		  paramsIn.lcid = m_lcid;
  		  m_pdcpSapUser->ReceivePdcpSdu (paramsIn);
  		  m_lastSubmittedRxSn = pdcpHeader.GetSequenceNumber();
  		  m_sdusBuffer.erase(it);
  	  }
	  /*for (auto it = m_sdusBuffer.begin(); it != m_sdusBuffer.end() && !m_sdusBuffer.empty() && it->first >= m_reorderingRxCount; ++it) {
	      NS_LOG_LOGIC (" sending up = " << it->first << ". Now expecting " << m_lastSubmittedRxSn + 1);
		  LtePdcpSapUser::ReceivePdcpSduParameters paramsIn;
		  it->second->RemoveHeader(pdcpHeader);
		  paramsIn.pdcpSdu = it->second;
		  paramsIn.rnti = m_rnti;
		  paramsIn.lcid = m_lcid;
		  m_pdcpSapUser->ReceivePdcpSdu (paramsIn);
		  m_lastSubmittedRxSn = it->first;
		  m_sdusBuffer.erase(it);
	  }*/
	  //if(m_lastSubmittedRxSn > (m_rxHfn << 12)) { m_lastSubmittedRxSn = m_lastSubmittedRxSn - ((m_rxHfn-1) << 12);
  //}
  // m_lastSubmittedRxSn = m_rxSequenceNumber - 1;
  if(!m_sdusBuffer.empty()) {
	  m_reorderingTimer = Simulator::Schedule (m_reorderingTimerValue, &LtePdcp::ExpireReorderingTimer ,this);
	  m_reorderingRxCount =  (m_rxHfn << 12) + m_rxSequenceNumber;
  }
}


} // namespace ns3
