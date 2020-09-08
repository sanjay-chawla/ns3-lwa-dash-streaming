/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 University of Washington
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
 */

#include "ns3/log.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-route.h"
#include "ns3/node.h"
#include "ns3/ipv4-static-routing.h"
#include "ns3/ipv4-list-routing.h"
#include "ipv4-ue-list-routing.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("Ipv4UeListRouting");

NS_OBJECT_ENSURE_REGISTERED (Ipv4UeListRouting);

TypeId
Ipv4UeListRouting::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Ipv4UeListRouting")
    .SetParent<Ipv4ListRouting> ()
    .SetGroupName ("Internet")
    .AddConstructor<Ipv4UeListRouting> ()
  ;
  return tid;
}


Ipv4UeListRouting::Ipv4UeListRouting ()
  : m_ipv4 (0)
{
  NS_LOG_FUNCTION (this);
}

Ipv4UeListRouting::~Ipv4UeListRouting ()
{
  NS_LOG_FUNCTION (this);
}

void
Ipv4UeListRouting::PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
  NS_LOG_FUNCTION (this << stream);
  *stream->GetStream () << "Node: " << m_ipv4->GetObject<Node> ()->GetId ()
                        << ", Time: " << Now().As (unit)
                        << ", Local time: " << GetObject<Node> ()->GetLocalTime ().As (unit)
                        << ", Ipv4UeListRouting table" << std::endl;
  for (Ipv4RoutingProtocolList::const_iterator i = m_routingProtocols.begin ();
       i != m_routingProtocols.end (); i++)
    {
      *stream->GetStream () << "  Priority: " << (*i).first << " Protocol: " << (*i).second->GetInstanceTypeId () << std::endl;
      (*i).second->PrintRoutingTable (stream, unit);
    }
}


void
Ipv4UeListRouting::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  for (Ipv4RoutingProtocolList::iterator rprotoIter = m_routingProtocols.begin ();
       rprotoIter != m_routingProtocols.end (); rprotoIter++)
    {
      // Note:  Calling dispose on these protocols causes memory leak
      //        The routing protocols should not maintain a pointer to
      //        this object, so Dispose() shouldn't be necessary.
      (*rprotoIter).second = 0;
    }
  m_routingProtocols.clear ();
  m_ipv4 = 0;
}


Ptr<Ipv4Route>
Ipv4UeListRouting::RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, enum Socket::SocketErrno &sockerr)
{
  NS_LOG_FUNCTION (this << p << header.GetDestination () << header.GetSource () << oif << sockerr);
  Ptr<Ipv4Route> route;

  for (Ipv4RoutingProtocolList::const_iterator i = m_routingProtocols.begin ();
       i != m_routingProtocols.end (); i++)
    {
      NS_LOG_LOGIC ("Checking protocol " << (*i).second->GetInstanceTypeId () << " with priority " << (*i).first);
      NS_LOG_LOGIC ("Requesting source address for destination " << header.GetDestination ());
      route = (*i).second->RouteOutput (p, header, oif, sockerr);
      if (route)
        {
          NS_LOG_LOGIC ("Found route " << route);
          sockerr = Socket::ERROR_NOTERROR;
          return route;
        }
    }
  NS_LOG_LOGIC ("Done checking " << GetTypeId ());
  NS_LOG_LOGIC ("");
  sockerr = Socket::ERROR_NOROUTETOHOST;
  return 0;
}


void
Ipv4UeListRouting::DoInitialize (void)
{
  NS_LOG_FUNCTION (this);
  for (Ipv4RoutingProtocolList::iterator rprotoIter = m_routingProtocols.begin ();
       rprotoIter != m_routingProtocols.end (); rprotoIter++)
    {
      Ptr<Ipv4RoutingProtocol> protocol = (*rprotoIter).second;
      protocol->Initialize ();
    }
  Ipv4RoutingProtocol::DoInitialize ();
}


// Patterned after Linux ip_route_input and ip_route_input_slow
bool
Ipv4UeListRouting::RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                             UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                             LocalDeliverCallback lcb, ErrorCallback ecb)
{
  NS_LOG_FUNCTION (this << p << header << idev << &ucb << &mcb << &lcb << &ecb);
  bool retVal = false;
  NS_LOG_LOGIC ("RouteInput logic for node: " << m_ipv4->GetObject<Node> ()->GetId ());

  NS_ASSERT (m_ipv4 != 0);
  // Check if input device supports IP
  NS_ASSERT (m_ipv4->GetInterfaceForDevice (idev) >= 0);
  uint32_t iif = m_ipv4->GetInterfaceForDevice (idev);

  bool forwardCheck = m_ipv4->IsDestinationAddress (header.GetDestination (), iif);
  // Check UE forwarding here
  // NS_LOG_DEBUG("Inside custom forwarder: fwd check " << forwardCheck);
  if ((header.GetSource ()==Ipv4Address("10.1.3.2"))&&
          (header.GetDestination ()==Ipv4Address("10.1.1.1"))&&
          (forwardCheck == false))
    {
	  	  Ipv4Header fHeader = header;
	      fHeader.SetDestination (Ipv4Address("7.0.0.2"));
          fHeader.Print(std::cout);

          Ptr<Ipv4Route> route = Create<Ipv4Route> ();
          route->SetDestination(fHeader.GetDestination());
          route->SetSource(fHeader.GetSource());
          route->SetGateway(fHeader.GetDestination());
          route->SetOutputDevice(m_ipv4->GetNetDevice(1));
          ucb(route, p, fHeader);

          /*NS_LOG_DEBUG("Case 1: Route: " << route->GetSource() << " >> " << route->GetDestination()
        		  << " via " << route->GetGateway() << " with output on " << route -> GetOutputDevice());
*/
          return true;
      }

  if ((header.GetSource ()==Ipv4Address("10.1.3.2"))&&
          (header.GetDestination ()==Ipv4Address("7.0.0.2"))&&
          (forwardCheck == true))
    {
          Ipv4Header fHeader = header;
          fHeader.SetDestination (Ipv4Address("10.1.1.1"));

          Ptr<Ipv4Route> route = Create<Ipv4Route> ();
          route->SetDestination(fHeader.GetDestination());
          route->SetSource(fHeader.GetSource());
          route->SetGateway(fHeader.GetDestination());
          route->SetOutputDevice(m_ipv4->GetNetDevice(2));
          ucb(route, p, fHeader);

//          NS_LOG_DEBUG("Case 2: Route: " << route->GetSource() << " >> " << route->GetDestination()
//        		  << " via " << route->GetGateway() << " with output on " << route -> GetOutputDevice());
          return true;
      }


  if ((header.GetSource ()==Ipv4Address("10.1.1.1"))
          &&
          (header.GetDestination ()==Ipv4Address("10.1.3.2"))&&
          (forwardCheck == false))
    {
          Ipv4Header fHeader = header;
          fHeader.SetDestination (Ipv4Address("10.1.2.2"));

          Ptr<Ipv4Route> route = Create<Ipv4Route> ();
          route->SetDestination(fHeader.GetDestination());
          route->SetSource(fHeader.GetSource());
          route->SetGateway(fHeader.GetDestination());
          route->SetOutputDevice(m_ipv4->GetNetDevice(1));
          ucb(route, p, fHeader);

//          NS_LOG_DEBUG("Case 3: Route: " << route->GetSource() << " >> " << route->GetDestination()
//        		  << " via " << route->GetGateway() << " with output on " << route -> GetOutputDevice());
          return true;
      }

  if ((header.GetSource ()==Ipv4Address("10.1.1.1"))
          &&
          (header.GetDestination ()==Ipv4Address("10.1.2.2"))&&
          (forwardCheck == true))
    {
          Ipv4Header fHeader = header;
          fHeader.SetDestination (Ipv4Address("10.1.3.2"));
          //fHeader.SetSource (Ipv4Address("10.0.4.3"));

          Ptr<Ipv4Route> route = Create<Ipv4Route> ();
          route->SetDestination(fHeader.GetDestination());
          route->SetSource(fHeader.GetSource());
          route->SetGateway(fHeader.GetDestination());
          route->SetOutputDevice(m_ipv4->GetNetDevice(2));
          ucb(route, p, fHeader);

//          NS_LOG_DEBUG("Case 4: Route: " << route->GetSource() << " >> " << route->GetDestination()
//        		  << " via " << route->GetGateway() << " with output on " << route -> GetOutputDevice());
          return true;
      }


  //
  retVal = m_ipv4->IsDestinationAddress (header.GetDestination (), iif);
  if (retVal == true)
    {
      NS_LOG_LOGIC ("Address "<< header.GetDestination () << " is a match for local delivery");
      if (header.GetDestination ().IsMulticast ())
        {
          Ptr<Packet> packetCopy = p->Copy ();
          lcb (packetCopy, header, iif);
          retVal = true;
          // Fall through
        }
      else
        {
          lcb (p, header, iif);
          return true;
        }
    }
  // Check if input device supports IP forwarding
  if (m_ipv4->IsForwarding (iif) == false)
    {
      NS_LOG_LOGIC ("Forwarding disabled for this interface");
      ecb (p, header, Socket::ERROR_NOROUTETOHOST);
      return true;
    }
  // Next, try to find a route
  // If we have already delivered a packet locally (e.g. multicast)
  // we suppress further downstream local delivery by nulling the callback
  LocalDeliverCallback downstreamLcb = lcb;
  if (retVal == true)
    {
      downstreamLcb = MakeNullCallback<void, Ptr<const Packet>, const Ipv4Header &, uint32_t > ();
    }
  for (Ipv4RoutingProtocolList::const_iterator rprotoIter =
         m_routingProtocols.begin ();
       rprotoIter != m_routingProtocols.end ();
       rprotoIter++)
    {
      if ((*rprotoIter).second->RouteInput (p, header, idev, ucb, mcb, downstreamLcb, ecb))
        {
          NS_LOG_LOGIC ("Route found to forward packet in protocol " << (*rprotoIter).second->GetInstanceTypeId ().GetName ());
          return true;
        }
    }
  // No routing protocol has found a route.
  return retVal;
}

void
Ipv4UeListRouting::NotifyInterfaceUp (uint32_t interface)
{
  NS_LOG_FUNCTION (this << interface);
  for (Ipv4RoutingProtocolList::const_iterator rprotoIter =
         m_routingProtocols.begin ();
       rprotoIter != m_routingProtocols.end ();
       rprotoIter++)
    {
      (*rprotoIter).second->NotifyInterfaceUp (interface);
    }
}
void
Ipv4UeListRouting::NotifyInterfaceDown (uint32_t interface)
{
  NS_LOG_FUNCTION (this << interface);
  for (Ipv4RoutingProtocolList::const_iterator rprotoIter =
         m_routingProtocols.begin ();
       rprotoIter != m_routingProtocols.end ();
       rprotoIter++)
    {
      (*rprotoIter).second->NotifyInterfaceDown (interface);
    }
}
void
Ipv4UeListRouting::NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION (this << interface << address);
  for (Ipv4RoutingProtocolList::const_iterator rprotoIter =
         m_routingProtocols.begin ();
       rprotoIter != m_routingProtocols.end ();
       rprotoIter++)
    {
      (*rprotoIter).second->NotifyAddAddress (interface, address);
    }
}
void
Ipv4UeListRouting::NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION (this << interface << address);
  for (Ipv4RoutingProtocolList::const_iterator rprotoIter =
         m_routingProtocols.begin ();
       rprotoIter != m_routingProtocols.end ();
       rprotoIter++)
    {
      (*rprotoIter).second->NotifyRemoveAddress (interface, address);
    }
}
void
Ipv4UeListRouting::SetIpv4 (Ptr<Ipv4> ipv4)
{
  NS_LOG_FUNCTION (this << ipv4);
  NS_ASSERT (m_ipv4 == 0);
  for (Ipv4RoutingProtocolList::const_iterator rprotoIter =
         m_routingProtocols.begin ();
       rprotoIter != m_routingProtocols.end ();
       rprotoIter++)
    {
      (*rprotoIter).second->SetIpv4 (ipv4);
    }
  m_ipv4 = ipv4;
}

void
Ipv4UeListRouting::AddRoutingProtocol (Ptr<Ipv4RoutingProtocol> routingProtocol, int16_t priority)
{
  NS_LOG_FUNCTION (this << routingProtocol->GetInstanceTypeId () << priority);
  m_routingProtocols.push_back (std::make_pair (priority, routingProtocol));
  m_routingProtocols.sort ( Compare );
  if (m_ipv4 != 0)
    {
      routingProtocol->SetIpv4 (m_ipv4);
    }
}

uint32_t
Ipv4UeListRouting::GetNRoutingProtocols (void) const
{
  NS_LOG_FUNCTION (this);
  return m_routingProtocols.size ();
}

Ptr<Ipv4RoutingProtocol>
Ipv4UeListRouting::GetRoutingProtocol (uint32_t index, int16_t& priority) const
{
  NS_LOG_FUNCTION (this << index << priority);
  if (index > m_routingProtocols.size ())
    {
      NS_FATAL_ERROR ("Ipv4UeListRouting::GetRoutingProtocol():  index " << index << " out of range");
    }
  uint32_t i = 0;
  for (Ipv4RoutingProtocolList::const_iterator rprotoIter = m_routingProtocols.begin ();
       rprotoIter != m_routingProtocols.end (); rprotoIter++, i++)
    {
      if (i == index)
        {
          priority = (*rprotoIter).first;
          return (*rprotoIter).second;
        }
    }
  return 0;
}

bool
Ipv4UeListRouting::Compare (const Ipv4RoutingProtocolEntry& a, const Ipv4RoutingProtocolEntry& b)
{
  NS_LOG_FUNCTION_NOARGS ();
  return a.first > b.first;
}

} // namespace ns3

