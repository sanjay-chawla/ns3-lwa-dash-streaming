/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 * Author: Sanjay Chawla (schawla@tcd.ie)
 */

//
//
//  +----------+                                                                +----------+
//  | virtual  |                                                                | virtual  |
//  |  Linux   |                                                                |  Linux   |
//  |   Host   |                                                                |   Host   |
//  |          |                                                                |          |
//  |   eth0   |                                                                |   eth0   |
//  +----------+                                                                +----------+
//       |                                                                           |
//  +----------+                                                                +----------+
//  |  Linux   |                                                                |  Linux   |
//  |  Bridge  |                                                                |  Bridge  |
//  +----------+                                                                +----------+
//       |                                                                           |
//  +------------+                                                            +-------------+
//  | "tap-left" |                                                            | "tap-right" |
//  +------------+                                                            +-------------+
//       |           n0             n1                       n2         n3            |
//       |       +--------+     +-------+                +-------+   +--------+       |
//       +-------|  tap   |     |  UE   |    +-----+     | Host  |   |  tap   |-------+
//               | bridge |     |       |----| NS3 |-----|       |   | bridge |
//               +--------+     +-------+    | LTE |     +-------+   +--------+
//               |  CSMA  |=====| CSMA  |    +-----+     | CSMA  |===|  CSMA  |
//               +--------+     +-------+                +-------+   +--------+
//
//
//
//
//
//
#include <iostream>
#include <fstream>

#include "ns3/core-module.h"
#include "ns3/config-store-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/tap-bridge-module.h"
#include "ns3/internet-module.h"
#include "ns3/node-throughput-tracer.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ipv4-ue-list-routing-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"

using namespace std;
using namespace ns3;

#include <unistd.h>
#include <iostream>

std::string GetCurrentWorkingDir( void ) {
  char buff[250];
  char* cwd = getcwd( buff, 250 );
  std::cout << cwd;
  std::string current_working_dir(buff);
  return current_working_dir;
}

NS_LOG_COMPONENT_DEFINE ("LteTapVirtualDevices");

int 
main (int argc, char *argv[])
{
  // Enable necessary logs
  LogComponentEnable("LteTapVirtualDevices", LOG_LEVEL_ALL);
  LogComponentEnable("Ipv4UeListRouting", LOG_LEVEL_ALL);
  LogComponentEnable("TraceHelper", LOG_LEVEL_ALL);

  // using more than 1 slows down things
  uint16_t numberOfEnbs = 1;
  uint16_t numberOfClients = 1;

  CommandLine cmd;
  cmd.AddValue("numberOfEnbs", "Number of eNBs", numberOfEnbs);
  cmd.AddValue("numberOfClients", "Number of UEs in total", numberOfClients);
  cmd.Parse (argc, argv);

  GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));
  Config::SetDefault("ns3::RealtimeSimulatorImpl::SynchronizationMode", StringValue("HardLimit"));
  Config::SetDefault("ns3::RealtimeSimulatorImpl::HardLimit", TimeValue(Seconds(60)));

  Config::SetDefault("ns3::CsmaChannel::DataRate", StringValue("100Gbps"));
  // P2P config
  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue (1450)); // 1460 is too large it seems
  // Configure Network Speed
  Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("100Gbps"));
  Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("5ms"));
  // To speed things up
  Config::SetDefault ("ns3::LteSpectrumPhy::CtrlErrorModelEnabled", BooleanValue (false));
  Config::SetDefault ("ns3::LteSpectrumPhy::DataErrorModelEnabled", BooleanValue (false));

  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults ();

  //
  // Create two ghost nodes.  The first will represent the virtual machine host
  // on the right side of the network; and the second will represent the VM client on
  // the left side.
  // Since the interface at UE and host is not CSMA we can not add a tap there.
  // Hence, create an additional hop
  //
  NodeContainer nodes;
  nodes.Create (4);

  NodeContainer nodesLeft;
  nodesLeft.Add (nodes.Get(0));
  nodesLeft.Add (nodes.Get(1));

  NodeContainer nodesRight;
  nodesRight.Add (nodes.Get(2));
  nodesRight.Add (nodes.Get(3));

  CsmaHelper csmaClient;
  csmaClient.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  csmaClient.SetChannelAttribute ("Delay", TimeValue (Seconds (0.001)));

  CsmaHelper csmaServer;
  csmaServer.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  csmaServer.SetChannelAttribute ("Delay", TimeValue (Seconds (0.001)));

  NetDeviceContainer devicesLeft = csmaClient.Install (nodesLeft);
  NetDeviceContainer devicesRight = csmaServer.Install (nodesRight);

  // We need a custom route forwarding logic at UE and host endpoints
  // (PGW only forwards to UE as it doesn't know other networks)
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ipv4UeListRoutingHelper list;
  list.Add (ipv4RoutingHelper, 0);

  InternetStackHelper internetLeft;
  internetLeft.SetRoutingHelper(list);
  internetLeft.Install (nodesLeft);

  InternetStackHelper internetRight;
  internetRight.SetRoutingHelper(list);
  internetRight.Install (nodesRight);

  // Lets configure LTE
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

  Ptr<Node> pgw = epcHelper->GetPgwNode ();

   // Make one of the nodes a host
  Ptr<Node> remoteHost = nodes.Get (2);

  // Create the Internet
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (5)));
  NetDeviceContainer hostDevices = p2ph.Install (pgw, remoteHost);

  Ipv4AddressHelper ipv4P2P;
  ipv4P2P.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesP2P = ipv4P2P.Assign (hostDevices);

  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("10.1.3.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("10.1.3.2"), 2);
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("10.1.1.0"), Ipv4Mask ("255.255.255.0"), epcHelper->GetUeDefaultGatewayAddress(), 1);

  NodeContainer remainingUeNodes;
  remainingUeNodes.Create(numberOfClients - 1); // One UE is connected to tap
  NodeContainer ueNodes;
  NodeContainer enbNodes;
  enbNodes.Create (numberOfEnbs);
  ueNodes.Add (nodes.Get(1));
  ueNodes.Add (remainingUeNodes);


  // Install Mobility Model
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  for (uint16_t i = 0; i < numberOfEnbs; i++)
    {
	  positionAlloc->Add (Vector(0, 0, 0));
    }
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.SetPositionAllocator(positionAlloc);
  mobility.Install(enbNodes);
  mobility.Install(ueNodes);

  // Install LTE Devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  Ipv4InterfaceContainer ueIpIface;
  InternetStackHelper internet;
  internet.Install (remainingUeNodes);
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));
  // Assign IP address to UEs, and install applications
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ueNode = ueNodes.Get (u);
      // Set the default gateway for the UE
      Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

  // Attach one UE per eNodeB
  /*for (uint16_t i = 0; i < numberOfClients; i++)
    {
      lteHelper->Attach (ueLteDevs.Get(i), enbLteDevs.Get(i));
      // side effect: the default EPS bearer will be activated
    }
   */
  lteHelper->Attach (ueLteDevs);

  Ipv4AddressHelper ipv4Left;
  ipv4Left.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesLeft = ipv4Left.Assign (devicesLeft);

  Ipv4AddressHelper ipv4Right;
  ipv4Right.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesRight = ipv4Right.Assign (devicesRight);
  NS_LOG_UNCOND("Installing Routing Tables");
  
  lteHelper->EnableTraces ();

  AsciiTraceHelper ascii;
  // p2ph.EnableAsciiAll (ascii.CreateFileStream ("lte-tap-virtual-device.tr"));
  p2ph.EnablePcapAll ("demo-lte");
  // Ipv4RoutingHelper::PrintRoutingTableAllAt(Seconds (5), ascii.CreateFileStream ("lteRoutingTable.txt"), Time::S);
  
  // Installing bridges
  TapBridgeHelper tapBridgeClient;
  tapBridgeClient.SetAttribute ("Mode", StringValue ("UseBridge"));
  tapBridgeClient.SetAttribute ("Mtu", StringValue ("1450"));
  tapBridgeClient.SetAttribute ("DeviceName", StringValue ("tap-left"));
  tapBridgeClient.Install (nodesLeft.Get (0), devicesLeft.Get (0));

  TapBridgeHelper tapBridgeServer;
  tapBridgeServer.SetAttribute ("Mode", StringValue ("UseBridge"));
  tapBridgeServer.SetAttribute ("Mtu", StringValue ("1450"));
  tapBridgeServer.SetAttribute ("DeviceName", StringValue ("tap-right"));
  tapBridgeServer.Install (nodesRight.Get (1), devicesRight.Get (1));

  //
  // Run the simulation for ten minutes
  //
  Simulator::Stop (Seconds (600.));
  ns3::AnimationInterface *anim;
  anim = new AnimationInterface ("animation.xml");
  anim->AddResource("");
    
  Simulator::Run ();
  Simulator::Destroy ();
}
