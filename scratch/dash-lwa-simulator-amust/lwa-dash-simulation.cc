/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2019 National Instruments
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
 * Author: Vincent Kotzsch <vincent.kotzsch@ni.com>
 *         Clemens Felber <clemens.felber@ni.com>
 */


#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cassert>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/tap-bridge-module.h"
#include "ns3/netanim-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/wifi-module.h"
#include "ns3/lte-helper.h"
#include "ns3/lte-module.h"
#include "ns3/csma-module.h"
#include "ns3/epc-helper.h"
#include "ns3/network-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/virtual-net-device.h"
#include "ns3/tap-bridge-module.h"
#include "ns3/AMuSt-module.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("NiLwaLwipExt");
std::map<uint16_t, Ptr<Socket>> lwaSocketMap;
std::map<string, Ptr<Socket>> contextSocketMap;
std::string outputDir;

// function for lwa/lwip packet handling transmitted through the LtePdcp::DoTransmitPdcpSdu
void LtePdcpLwaHandler (LteRlcSapProvider::TransmitPdcpPduParameters params){

  //NS_LOG_DEBUG("rnti : " << params.rnti);

  // copy incoming packet
  Ptr<Packet> currentPacket = params.pdcpPdu->Copy();

  // remove headers to get sequence number and include lwa/lwip header
  LtePdcpHeader pdcpHeader;
  currentPacket->RemoveHeader (pdcpHeader);
  Ipv4Header ipHeader;
  currentPacket->RemoveHeader (ipHeader);
  TcpHeader tcpHeader;
  currentPacket->RemoveHeader (tcpHeader);

  // add headers again
  currentPacket->AddHeader (tcpHeader);
  currentPacket->AddHeader (ipHeader);
  currentPacket->AddHeader (pdcpHeader);

  // read lwa tags added by pdcp layer and add lwa information
  // as separate header to data packets (won't be removed when transmitting
  // packets via API)
  LwaTag   lwaTag;
  PdcpLcid lcidTag;
  uint32_t lcid=0;
  uint32_t bid=0;
  LwaLwipHeader lwaLwipHeader;

  // tag to copy the logical channel id of the frame
  if(currentPacket->FindFirstMatchingByteTag (lcidTag)){
      lcid = lcidTag.Get();
      bid = lcid - 2;
  }
  lwaLwipHeader.SetBearerId (bid);

  // packet handling for lwa packets
  if (currentPacket->PeekPacketTag(lwaTag)){
      //copy the status of LWA activation (LTE, Split, swtiched)
      lwaLwipHeader.SetLwaActivate (lwaTag.Get());
      // add lwa lwip header
      currentPacket->AddHeader (lwaLwipHeader);
      lwaSocketMap.find(params.rnti)->second->Send(currentPacket);
	  //NS_LOG_DEBUG ("LWA: Sent packet with PDCP Sequence Number " << pdcpHeader.GetSequenceNumber());
  }
}

// assign callback function to the pdcp object
// NOTE: object is created after bearer release which is after attach procedure
static void
Callback_LtePDCPTX
(uint16_t rnti){
	std::string rntiPath = "/NodeList/*/DeviceList/*/$ns3::LteNetDevice/$ns3::LteEnbNetDevice/LteEnbRrc/UeMap/" + to_string(rnti) + "/DataRadioBearerMap/*/LtePdcp/TxPDUtrace";
  Config::ConnectWithoutContext (rntiPath, MakeCallback (&LtePdcpLwaHandler));
}

void
NotifyConnectionEstablishedUe (
					   string context,
                       uint64_t imsi,
                       uint16_t cellId,
                       uint16_t rnti)
{
  std::cout << Simulator::Now ().GetSeconds ()
            << " UE IMSI " << imsi
            << ": connected to CellId " << cellId
            << " with RNTI " << rnti
			<< " context " << context
            << std::endl;
  // create socket on lwaap socket to enable transmission of lwa packets
/*
          Ptr<Socket> lwaapTxSocket = Socket::CreateSocket (lwaapNode, tid);
          lwaapTxSocket->Bind ();
          lwaapTxSocket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom(wifiIpInterfaces.GetAddress (i+1)), 9));
          lwaapTxSocket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
          lwaapTxSocket->SetAllowBroadcast (true);

          //TODO: temporary fix assuming rnti=i+1
          lwaSocketMap.insert(std::pair<uint16_t, Ptr<Socket>>(i+1, lwaapTxSocket));
  	  NS_LOG_INFO("STA Socket bound " << i+1);
*/
  lwaSocketMap.insert(std::pair<uint16_t, Ptr<Socket>>(rnti, contextSocketMap.find(context)->second));
  Simulator::Schedule(MilliSeconds(100), &Callback_LtePDCPTX, rnti);
}

std::string GetCurrentWorkingDir( void ) {
  char buff[250];
  char* cwd = getcwd( buff, 250 );
  std::cout << cwd;
  std::string current_working_dir(buff);
  return current_working_dir;
}

ApplicationContainer GetDASHClientApplication (string ssMPDURL, Ptr<Node> ueNode, double simTime, uint32_t remoteHostId){
	DASHHttpClientHelper client(ssMPDURL);
	static std::string AdaptionLogicArray[] = {
			"RateBasedAdaptationLogic",
			"DASHJSAdaptationLogic",
			"BufferBasedAdaptationLogic",
			"AlwaysLowestAdaptationLogic",
			"RateAndBufferBasedAdaptationLogic"
	};
	static std::string DeviceTypeArray[] = {"PC","Laptop","Tablet","Mobile"};
	static int ScreenWidthArray[] = {1920,1240,1024,800};
	static int ScreenHeightArray[] = {1080,720,600,480};

	int deviceConfig = rand() % 4;

	client.SetAttribute("AdaptationLogic", StringValue("dash::player::" + AdaptionLogicArray[1])); // Always use DASHJSAdaptationLogic
	// client.SetAttribute("StartUpDelay", DoubleValue(2.0)); // delay of 2 sec
	client.SetAttribute("ScreenWidth", UintegerValue(ScreenWidthArray[deviceConfig]));
	client.SetAttribute("ScreenHeight", UintegerValue(ScreenHeightArray[deviceConfig]));
	client.SetAttribute("AllowDownscale", BooleanValue(true));
	client.SetAttribute("AllowUpscale", BooleanValue(true));
	client.SetAttribute("MaxBufferedSeconds", StringValue("60"));
	client.SetAttribute("DeviceType", StringValue(DeviceTypeArray[deviceConfig]));

	ofstream request_file;
	request_file.open(outputDir + "demo_req.csv", std::ios_base::app); // append instead of overwrite

	request_file << ueNode->GetId() << "," << remoteHostId << "," << 2 << "," << simTime << "," << "0,3337" << "," << ScreenWidthArray[deviceConfig] << "," << ScreenHeightArray[deviceConfig] << "\n";
	return client.Install(ueNode);
}

// main function
int main (int argc, char *argv[]){

  bool verbose = true;

  // enable native ns-3 logging - note: can also be used if compiled in "debug" mode
  if (verbose)
    {
	  PacketMetadata::Enable();
      //LogComponentEnable ("LtePdcp", LOG_LEVEL_ALL);
      LogComponentEnable ("NiLwaLwipExt", LOG_LEVEL_ALL);
      //LogComponentEnable("TcpSocketBase", LOG_LEVEL_LOGIC);
      //LogComponentEnable("Ipv4LwaUeStaticRouting", LOG_LEVEL_ALL);
      //LogComponentEnable("LwaApplication", LOG_LEVEL_ALL);
      //LogComponentEnable("Config", LOG_LEVEL_ALL);
      //LogComponentEnable("HttpClientApplication", LOG_LEVEL_ALL);
      //LogComponentEnable("ThreeGppHttpServer", LOG_LEVEL_ALL);
      //LogComponentEnable("StaWifiMac", LOG_LEVEL_LOGIC);
    }

  // Simulation time in seconds
  double simTime    = 60;
  uint8_t nUes = 4;
  uint8_t dashSegmentDuration = 15;
  double lwaactivate=1;     // LTE+Wifi=1, Wi-Fi=2



  //
  double rss = -80; // -dBm

  // =======================
  // UOC LWIP/LWA extensions



  // MTU size for P2P link between EnB and Wifi AP
  double xwLwaLinkMtuSize=1500;
  //delay of p2p link
  std::string xwLwaLinkDelay="0ms";
  //data rate of p2p link
  std::string xwLwaLinkDataRate="100Mbps";

  // ==========================================
  // parse command line for function parameters

  CommandLine cmd;

  cmd.AddValue("simTime", "Duration in seconds which the simulation should run", simTime);
  cmd.AddValue("nUes", "Number of UEs", nUes);
  cmd.AddValue("dashSegmentDuration", "Duration in seconds of DASH segments", dashSegmentDuration);
  cmd.AddValue("lwaactivate", "Activate LWA (0-LTE, 1-LWA)",lwaactivate);

  cmd.Parse (argc, argv);

  // Activate the ns-3 real time simulator
  GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::DefaultSimulatorImpl"));
  //Config::SetDefault ("ns3::RealtimeSimulatorImpl::SynchronizationMode", StringValue("BestEffort")); //BestEffort
  // GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true)); // needed for tapBridge

  // p2p helper
  PointToPointHelper p2pHelp;
                     p2pHelp.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
                     p2pHelp.SetChannelAttribute ("Delay", StringValue ("2ms"));
                     p2pHelp.SetDeviceAttribute ("Mtu", UintegerValue (1500));


  // wifi helper
  Ssid       ssid = Ssid ("wifi-default");

  WifiHelper wifiHelp;
             wifiHelp.SetStandard (WIFI_PHY_STANDARD_80211ac);
             wifiHelp.SetRemoteStationManager ("ns3::ConstantRateWifiManager");
  //if (verbose) wifiHelp.EnableLogComponents ();  // Turn on all Wifi logging

  WifiMacHelper wifiApMacHelp, wifiStaMacHelp ;
  wifiApMacHelp.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
  wifiStaMacHelp.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid));

  // disable fragmentation for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  // turn off RTS/CTS for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));

  Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue (80));

  YansWifiChannelHelper wifiChannelHelp = YansWifiChannelHelper::Default ();
                        wifiChannelHelp.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
                        wifiChannelHelp.AddPropagationLoss ("ns3::FixedRssLossModel","Rss",DoubleValue (rss));
  YansWifiPhyHelper     wifiPhyHelp =  YansWifiPhyHelper::Default ();
                        wifiPhyHelp.Set ("RxGain", DoubleValue (0) );
                        wifiPhyHelp.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
                        wifiPhyHelp.SetChannel (wifiChannelHelp.Create ());

  MobilityHelper MobilityHelp;
                 MobilityHelp.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  // ip helper
  InternetStackHelper     ipStackHelp;//, ueIpStackHelp;
  Ipv4AddressHelper       ipAddressHelp;
  Ipv4Mask                IpMask = "255.255.255.0";
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  //Ipv4LwaUeStaticRoutingHelper ipv4LwaUeRoutingHelper;
  //ueIpStackHelp.SetRoutingHelper(ipv4LwaUeRoutingHelper);
  // ipv4LwaUeRoutingHelper.Add(ipv4RoutingHelper, 0);

  // main router node to connect lan with wifi and lte mobile networks
  Ptr<Node> MobileNetworkGwNode = CreateObject<Node> ();
  Names::Add("RemoteHost", MobileNetworkGwNode);
  // wifi station nodes
  NodeContainer wifiStaNodes;
                wifiStaNodes.Create (0);
  // Names::Add("staNode", wifiStaNodes.Get(0));
  // wifi access point nodes - here fixed to one but can be extended
  NodeContainer wifiApNodes;
                wifiApNodes.Create (2);
  // Names::Add("ApNode", wifiApNodes.Get(0));
  // lan nodes connected via ethernet - added mobile network gateway here
  NodeContainer LanNodes;
                LanNodes.Add (MobileNetworkGwNode);
                //LanNodes.Create (2);

  // lte user terminal nodes
  NodeContainer ueNodes;
                ueNodes.Create(nUes);
  Names::Add("UENode", ueNodes.Get(0));
  // lte enb nodes - here fixed to one but can be extended
  NodeContainer enbNodes;
                enbNodes.Create(1);
  Names::Add("EnbNode", enbNodes.Get(0));

  std::stringstream outputDirStream;
    if (lwaactivate==1)      outputDirStream << "output/lwa/dualstripe/ues_" << to_string(nUes) << "_" << to_string(dashSegmentDuration) << "s/";
    else if (lwaactivate==2) outputDirStream << "output/wifi/dualstripe/ues_" << to_string(nUes) << "_" << to_string(dashSegmentDuration) << "s/";
    else                     outputDirStream << "output/lte/dualstripe/ues_" << to_string(nUes) << "_" << to_string(dashSegmentDuration) << "s/";

  //outputDirStream << "output/lte/dualstripe/ues_" << nUes << "_" << dashSegmentDuration << "s/";
  outputDir = outputDirStream.str();
  ns3::SystemPath::MakeDirectories(GetCurrentWorkingDir() + "/" +  outputDirStream.str());

  // install corresponding net devices on all nodes
  // NetDeviceContainer p2pWifiDevices  = p2pHelp.Install (MobileNetworkGwNode, wifiApNode.Get(0));

  //NetDeviceContainer csmaDevices     = csmaHelp.Install (LanNodes);
  // note: all ue nodes have an lte and wifi net device
  NetDeviceContainer staDevices      = wifiHelp.Install (wifiPhyHelp, wifiStaMacHelp, NodeContainer(ueNodes, wifiStaNodes));
  NetDeviceContainer apDevices       = wifiHelp.Install (wifiPhyHelp, wifiApMacHelp, wifiApNodes);
/*
  Ssid       ssid2 = Ssid ("wifi-default-2");
  wifiApMacHelp.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid2));
  wifiStaMacHelp.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid2));
  apDevices.Add(wifiHelp.Install (wifiPhyHelp, wifiApMacHelp, wifiApNodes.Get(1)));
  staDevices.Add(wifiHelp.Install (wifiPhyHelp, wifiStaMacHelp, NodeContainer(ueNodes.Get(1), wifiStaNodes)));
*/

  // install ip stacks on all nodes
  ipStackHelp.Install (LanNodes);
  ipStackHelp.Install (wifiStaNodes);
  ipStackHelp.Install (wifiApNodes);
  ipStackHelp.Install (ueNodes);
  //ueIpStackHelp.Install (ueNodes);

  // configure ip address spaces and gateways for the different subnetworks
  // Ipv4Address csmaIpSubnet    = "10.1.1.0";
  Ipv4Address WifiIpSubnet    = "10.1.2.0";
  // Ipv4Address p2pWifiIpSubnet = "10.1.3.0";
  Ipv4Address p2pLteIpSubnet  = "10.1.4.0";
  Ipv4Address UeIpSubnet      = "7.0.0.0";
  Ipv4Address xwLwaSubnet     = "20.1.2.0";
  //Ipv4Address xwLwipSubnet    = "20.1.3.0";

  // create wifi p2p link to mobile network gateway
  /*ipAddressHelp.SetBase (p2pWifiIpSubnet, IpMask);
  Ipv4InterfaceContainer p2pWifiIpInterfaces = ipAddressHelp.Assign (p2pWifiDevices);
  Ipv4Address LanGwIpAddr = p2pWifiIpInterfaces.GetAddress (0);
*/
  // create lan network with stations - sta0 is mobile network gateway
  /*ipAddressHelp.SetBase (csmaIpSubnet, IpMask);
  Ipv4InterfaceContainer LanIpInterfaces = ipAddressHelp.Assign (csmaDevices);
  Ipv4Address LanGwIpAddr = LanIpInterfaces.GetAddress (0);//"10.1.1.1";
  */

  // create wifi network - ap0 is connected to mobile network gateway
  ipAddressHelp.SetBase (WifiIpSubnet, IpMask);
  Ipv4InterfaceContainer wifiIpInterfaces;
                         wifiIpInterfaces.Add (ipAddressHelp.Assign (apDevices));
                         wifiIpInterfaces.Add (ipAddressHelp.Assign (staDevices));
  Ipv4Address WifiGwIpAddr = wifiIpInterfaces.GetAddress (0);//"10.1.2.1";

  // ==========================
  // LWA/LWIP extensions by UOC

  // Switch to enable/disable LWA functionality (also partial use)
  Config::SetDefault ("ns3::LtePdcp::PDCPDecLwa", UintegerValue(lwaactivate));

  // create p2p helper for LWA/Xw link
  PointToPointHelper p2pHelpXw;
                     p2pHelpXw.SetDeviceAttribute ("DataRate", StringValue (xwLwaLinkDataRate));
                     p2pHelpXw.SetChannelAttribute ("Delay", StringValue (xwLwaLinkDelay));
                     p2pHelpXw.SetDeviceAttribute ("Mtu", UintegerValue (xwLwaLinkMtuSize));

  // create lwaap node
  Ptr<Node> lwaapNode = CreateObject<Node> ();
  Names::Add("LWAAP", lwaapNode);
  // install p2p net devices to create parallel lwa and lwip links
  NetDeviceContainer xwLwaDevices  = p2pHelpXw.Install (lwaapNode, wifiApNodes.Get(0));
  xwLwaDevices.Add(p2pHelpXw.Install (lwaapNode, wifiApNodes.Get(1)));

  // install ip stack on additional nodes
  ipStackHelp.Install(lwaapNode);

  // assign ip adresses to lwa / lwip links
  Ipv4AddressHelper XwAddress;
  XwAddress.SetBase (xwLwaSubnet, "255.255.255.0");
  Ipv4InterfaceContainer xwLwaIpInterfaces = XwAddress.Assign (xwLwaDevices);


  // schedule function call to connect locally defined Lte_pdcpDlTxPDU traceback function to LTE PDCP Tx
  // Important: First function call call needs to be scheduled AFTER initial attach procedure is finished successfully.
  //            Thats why a callbackStart delay is added here as a dirty hack
  // TODO-NI: find a better solution to connect callback to PDCP (without timing constaint)
  /*const uint32_t assumedLteAttachDelayMs = 1000;
  if ((transmTime * 1000) < assumedLteAttachDelayMs) {
      // limit start time for packet generation
      transmTime = assumedLteAttachDelayMs / 1000;
      NS_LOG_DEBUG ("StartTime for packet generation too small! Adapted to: " << transmTime << " second(s)");
  }
  // start call back 100ms before datageneration
  uint32_t callbackStartMs = uint32_t (transmTime * 1000 - 100);
  Simulator::Schedule (MilliSeconds(callbackStartMs),&Callback_LtePDCPTX);*/

  // initialize routing database and set up the routing tables in the nodes
  // Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // ===================================================================================================================
  // setup the lte network - note that lte is configured after "PopulateRoutingTables" as static routing is used for epc

  // Set downlink transmission bandwidth in number of resource blocks -> set to 20MHz default here
  Config::SetDefault ("ns3::LteEnbNetDevice::DlBandwidth", UintegerValue (100));
  // Disable ideal and use real RRC protocol with RRC PDUs (cf https://www.nsnam.org/docs/models/html/lte-design.html#real-rrc-protocol-model)
  Config::SetDefault ("ns3::LteHelper::UseIdealRrc", BooleanValue (false));
  // Disable CQI measurement reports based on PDSCH (based on ns-3 interference model)
  Config::SetDefault ("ns3::LteHelper::UsePdschForCqiGeneration", BooleanValue (false));
  // Use simple round robin scheduler here (cf https://www.nsnam.org/docs/models/html/lte-design.html#round-robin-rr-scheduler)
  Config::SetDefault ("ns3::LteHelper::Scheduler", StringValue ("ns3::RrFfMacScheduler"));
  // Set the adpative coding and modulation model to Piro as this is the simpler one
  Config::SetDefault ("ns3::LteAmc::AmcModel", EnumValue (LteAmc::PiroEW2010));
  // Disable HARQ as this not support by PHY and not implemented in NI API
  Config::SetDefault ("ns3::RrFfMacScheduler::HarqEnabled", BooleanValue (false));
  // Set CQI timer threshold in sec - depends on CQI report frequency to be set in NiLtePhyInterface
  Config::SetDefault ("ns3::RrFfMacScheduler::CqiTimerThreshold", UintegerValue (1000));
  // Set the length of the window (in TTIs) for the reception of the random access response (RAR); the resulting RAR timeout is this value + 3 ms
  Config::SetDefault ("ns3::LteEnbMac::RaResponseWindowSize", UintegerValue(10));

  // Config::ConnectWithoutContext ("/NodeList/*/DeviceList/*/LteUeRrc/ConnectionEstablished", MakeCallback (&NotifyConnectionEstablishedUe));
// Set ConnectionTimeoutDuration, after a RA attempt, if no RRC Connection Request is received before this time, the UE context is destroyed.
  // Must account for reception of RAR and transmission of RRC CONNECTION REQUEST over UL GRANT.
  //Config::SetDefault ("ns3::LteEnbRrc::ConnectionRequestTimeoutDuration", StringValue ("+60000000.0ns"));


  // epc helper is used to create core network incl installation of eNB App
  Ptr<PointToPointEpcHelper>  epcHelper = CreateObject<PointToPointEpcHelper> ();
  // lte helper is used to install L2/L3 on eNB/UE (PHY,MAC,RRC->PDCP/RLC, eNB NetDevice)
  Ptr<LteHelper>              lteHelper = CreateObject<LteHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

  // Use simple round robin scheduler here
  // (cf https://www.nsnam.org/docs/models/html/lte-design.html#round-robin-rr-scheduler)
  lteHelper->SetSchedulerType ("ns3::RrFfMacScheduler");

  Ptr<Node> PacketGwNode = epcHelper->GetPgwNode ();
  Names::Add("PGW", PacketGwNode);
  // create lte p2p link to mobile network gateway - work around for the global/static routing problem
  NetDeviceContainer p2pLteDevices = p2pHelp.Install (MobileNetworkGwNode, PacketGwNode);
  // assign ip adresses for p2p link
  ipAddressHelp.SetBase (p2pLteIpSubnet, IpMask);
  Ipv4InterfaceContainer p2pLteIpInterfaces = ipAddressHelp.Assign (p2pLteDevices);
  Ipv4Address LanGwIpAddr = p2pLteIpInterfaces.GetAddress (0);

  // lte & wifi channel / mobility  model
  MobilityHelp.Install (wifiStaNodes);
  MobilityHelp.Install (wifiApNodes);
  MobilityHelp.Install (enbNodes);
  MobilityHelp.Install (ueNodes);

  // Install lte net devices to the nodes
  NetDeviceContainer enbDevices = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueDevices  = lteHelper->InstallUeDevice (ueNodes);

  // Install the ip stack on the ue nodes
  Ipv4InterfaceContainer ueIpInterfaces = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueDevices));

  // Set the default gateway for the lte ue nodes - will be used for all outgoing packets for this node
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> TmpNode = ueNodes.Get (u);
      Ptr<Ipv4StaticRouting> StaticRouting = ipv4RoutingHelper.GetStaticRouting (TmpNode->GetObject<Ipv4> ());
      //Ptr<Ipv4LwaUeStaticRouting> StaticRouting = ipv4LwaUeRoutingHelper.GetStaticRouting (TmpNode->GetObject<Ipv4> ());
      StaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 2);
      //StaticRouting->SetDefaultRoute (Ipv4Address (WifiGwIpAddr), 1);
      StaticRouting->AddNetworkRouteTo (Ipv4Address (WifiIpSubnet), Ipv4Mask (IpMask),  Ipv4Address (WifiGwIpAddr), 1);
      StaticRouting->AddNetworkRouteTo (Ipv4Address (xwLwaSubnet), Ipv4Mask (IpMask),  Ipv4Address (WifiGwIpAddr), 1);
      StaticRouting->AddNetworkRouteTo (Ipv4Address (WifiIpSubnet), Ipv4Mask (IpMask),  Ipv4Address (wifiIpInterfaces.GetAddress(1)), 1);
      StaticRouting->AddNetworkRouteTo (Ipv4Address (xwLwaSubnet), Ipv4Mask (IpMask),  Ipv4Address (wifiIpInterfaces.GetAddress(1)), 1);
    }

  lteHelper->AttachToClosestEnb(ueDevices, enbDevices);
  // Attach one UE per eNodeB
  for (uint16_t i = 0; i < ueNodes.GetN (); i++)
    {
      //lteHelper->Attach(ueDevices.Get(i));//Attach (ueDevices.Get(i), enbDevices.Get(0));
      Ptr<LteUeNetDevice> ueDev = ueDevices.Get(i)->GetObject<LteUeNetDevice> ();
      Ptr<LteUeRrc> ueRrc = ueDev->GetRrc ();

      //ueRrc->TraceConnectWithoutContext("ConnectionEstablished", MakeCallback(&NotifyConnectionEstablishedUe));
      ueRrc->TraceConnect("ConnectionEstablished", to_string(i), MakeCallback(&NotifyConnectionEstablishedUe));
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");

      Ptr<Socket> lwaapTxSocket = Socket::CreateSocket (lwaapNode, tid);
	  lwaapTxSocket->Bind ();
	  lwaapTxSocket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom(wifiIpInterfaces.GetAddress (i+2)), 9));
	  lwaapTxSocket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
	  lwaapTxSocket->SetAllowBroadcast (true);

	  //TODO: temporary fix assuming rnti=i+1
	  contextSocketMap.insert(std::pair<string, Ptr<Socket>>(to_string(i), lwaapTxSocket));

      Ptr<Socket> staRxSocket = Socket::CreateSocket (ueNodes.Get(i), tid);
      staRxSocket->Bind (InetSocketAddress (Ipv4Address::GetAny(), 9));
      ueRrc->m_staRxSocket = staRxSocket;
      }

  // Set the default gateway for the wifi stations
  for (uint32_t u = 0; u < wifiStaNodes.GetN (); ++u)
    {
      Ptr<Node> TmpNode = wifiStaNodes.Get (u);
      Ptr<Ipv4StaticRouting> StaticRouting = ipv4RoutingHelper.GetStaticRouting (TmpNode->GetObject<Ipv4> ());
      StaticRouting->SetDefaultRoute (Ipv4Address (WifiGwIpAddr), 1);
      //StaticRouting->AddNetworkRouteTo (Ipv4Address (UeIpSubnet), Ipv4Mask ("255.0.0.0"),  Ipv4Address (WifiGwIpAddr), 1);
    }

  // Set the default gateway for the lan stations
  for (uint32_t u = 1; u < LanNodes.GetN (); ++u)
    {
      Ptr<Node> TmpNode = LanNodes.Get (u);
      Ptr<Ipv4StaticRouting> StaticRouting = ipv4RoutingHelper.GetStaticRouting (TmpNode->GetObject<Ipv4> ());
      StaticRouting->SetDefaultRoute (Ipv4Address (LanGwIpAddr), 1);
      //StaticRouting->AddNetworkRouteTo (Ipv4Address (UeIpSubnet), Ipv4Mask ("255.0.0.0"),  Ipv4Address (LanGwIpAddr), 1);
    }

  // add route to lte network in wifi ap gateway
  Ptr<Ipv4StaticRouting> ApGwNodeStaticRouting = ipv4RoutingHelper.GetStaticRouting (wifiApNodes.Get (0)->GetObject<Ipv4> ());
  ApGwNodeStaticRouting->AddNetworkRouteTo (Ipv4Address (UeIpSubnet), Ipv4Mask ("255.0.0.0"), 1);
  ApGwNodeStaticRouting = ipv4RoutingHelper.GetStaticRouting (wifiApNodes.Get (1)->GetObject<Ipv4> ());
    ApGwNodeStaticRouting->AddNetworkRouteTo (Ipv4Address (UeIpSubnet), Ipv4Mask ("255.0.0.0"), 1);
  // add route to lte network in lan mobile gateway
  Ptr<Ipv4StaticRouting> MobileNetworkGwNodeStaticRouting = ipv4RoutingHelper.GetStaticRouting (MobileNetworkGwNode->GetObject<Ipv4> ());
  MobileNetworkGwNodeStaticRouting->AddNetworkRouteTo (Ipv4Address (UeIpSubnet), Ipv4Mask ("255.0.0.0"), 1);
  // MobileNetworkGwNodeStaticRouting->AddNetworkRouteTo (Ipv4Address (WifiIpSubnet), Ipv4Mask (IpMask), 2);
  // add route from lte network to lan and wifi networks in lte packet gateway
  Ptr<Ipv4StaticRouting> pgwStaticRouting = ipv4RoutingHelper.GetStaticRouting (PacketGwNode->GetObject<Ipv4> ());
  // pgwStaticRouting->AddNetworkRouteTo (Ipv4Address (csmaIpSubnet), Ipv4Mask (IpMask), 2);
  pgwStaticRouting->AddNetworkRouteTo (Ipv4Address (WifiIpSubnet), Ipv4Mask (IpMask), 2);

  Ptr<Ipv4StaticRouting> lwaapRouting = ipv4RoutingHelper.GetStaticRouting (lwaapNode->GetObject<Ipv4> ());
  //lwaapRouting->SetDefaultRoute (Ipv4Address (wifiIpInterfaces.GetAddress(1)), 1);
  //StaticRouting->SetDefaultRoute (Ipv4Address (WifiGwIpAddr), 1);
  lwaapRouting->AddNetworkRouteTo (Ipv4Address (WifiGwIpAddr), Ipv4Mask (IpMask), 1);
  lwaapRouting->AddNetworkRouteTo (Ipv4Address (UeIpSubnet), Ipv4Mask (IpMask),  Ipv4Address (WifiGwIpAddr), 1);
  lwaapRouting->AddNetworkRouteTo (Ipv4Address (UeIpSubnet), Ipv4Mask (IpMask),  Ipv4Address (wifiIpInterfaces.GetAddress(1)), 1);

  // define client and server nodes

  // user always LAN node as client
  NodeContainer ClientNode     = ueNodes.Get (0);
  //Ipv4Address   ClientIpAddr   = p2pLteIpInterfaces;
  NodeContainer ServerNode     = LanNodes.Get (0);
  Ipv4Address   ServerIPAddr   = LanGwIpAddr;

  // print addresses
  std::cout << std::endl;
  std::cout << "-------- NS-3 Topology Information ------" << std::endl;
  //std::cout << "Number of ETH devices      = " << LanIpInterfaces.GetN() << std::endl;
  std::cout << "Number of WiFi devices     = " << wifiIpInterfaces.GetN() << std::endl;
  std::cout << "Number of LTE UE devices   = " << ueIpInterfaces.GetN() << std::endl;
  std::cout << "Router GW IP Addr          = " << p2pLteIpInterfaces.GetAddress (1) << std::endl;
  //std::cout << "WiFi Net GW IP Addr        = " << p2pWifiIpInterfaces.GetAddress(1) << std::endl;
  std::cout << "WiFi AP IP Addr            = " << WifiGwIpAddr << std::endl;
  std::cout << "WiFI STA#1 IP Addr         = " << wifiIpInterfaces.GetAddress(2) << std::endl;
  std::cout << "LTE Net GW IP Addr         = " << p2pLteIpInterfaces.GetAddress(1) << std::endl;
  std::cout << "LTE EPC PGW IP Addr        = " << PacketGwNode->GetObject<Ipv4> ()->GetAddress (1,0).GetLocal () << std::endl;
  std::cout << "LTE UE#1 IP Addr           = " << ueIpInterfaces.GetAddress(0) << std::endl;
  std::cout << "Xw LWA IP Addr             = " << xwLwaIpInterfaces.GetAddress (0) << std::endl;
  //std::cout << "Xw LWIP IP Addr            = " << xwLwipIpInterfaces.GetAddress (0) << std::endl;
  //std::cout << "Client IP Addr             = " << ClientIpAddr << std::endl;
  std::cout << "Server IP Addr             = " << ServerIPAddr << std::endl;
  std::cout << std::endl;

  /* Install TCP/UDP Transmitter on the station */
  uint16_t port = 80;
  ofstream request_file;
   request_file.open(outputDir + "demo_req.csv", std::ios_base::out); // append instead of overwrite
   request_file << "ClientNode,ServerNode,StartsAt,StopsAt,VideoId,LinkCapacity,ScreenWidth,ScreenHeight\n";
   request_file.close();

  //Store IP adresses
  std::string addr_file=outputDir + "addresses";
  ofstream out_addr_file(addr_file.c_str());
  out_addr_file << MobileNetworkGwNode->GetId() <<  " " << ServerIPAddr << endl;

   std::stringstream ssMPDURL;
   std::stringstream dashPath;
   dashPath << "/DashWorkspace/bbb_" << std::to_string(dashSegmentDuration) << "s/";
   ssMPDURL << "http://" << ServerIPAddr << dashPath.str() << "vid1.mpd.gz";

	std::string representationStrings = GetCurrentWorkingDir() + "/../.." + dashPath.str() + "dash_dataset_avc_bbb.csv";
	fprintf(stderr, "representations = %s\n", representationStrings.c_str());
	DASHServerHelper server(Ipv4Address::GetAny(), port,  "10.1.4.1", dashPath.str(), representationStrings, dashPath.str());


  ApplicationContainer serverApp = server.Install (ServerNode);

  ApplicationContainer sinkApp;

  for (uint32_t u = 0; u < ueNodes.GetN (); ++u) {
	  Ptr<Node> ue =  ueNodes.Get(u);
	  sinkApp.Add(GetDASHClientApplication(ssMPDURL.str(), ue, simTime, MobileNetworkGwNode->GetId()));
	  for(uint32_t l=1;l<ue->GetObject<Ipv4> ()->GetNInterfaces();l++){
	    out_addr_file << ue->GetId() <<  " " << ue->GetObject<Ipv4> ()->GetAddress(l, 0).GetLocal() << endl;
	  }
	  /*
	  std::stringstream ssMPDURL;
	    ssMPDURL << "http://" << ServerIPAddr << "/content/segments/BigBuckBunny/bunny_2s/vid1.mpd.gz";
	    // NS_LOG_DEBUG(">>>>>>>>>>> add: " << ssMPDURL.str());
	    DASHHttpClientHelper client(ssMPDURL.str());
	    client.SetAttribute("AdaptationLogic", StringValue(AdaptationLogicToUse));
	    client.SetAttribute("StartUpDelay", StringValue("0.5"));
	    client.SetAttribute("ScreenWidth", UintegerValue(screenWidth));
	    client.SetAttribute("ScreenHeight", UintegerValue(screenHeight));
	    client.SetAttribute("AllowDownscale", BooleanValue(true));
	    client.SetAttribute("AllowUpscale", BooleanValue(true));
	    client.SetAttribute("MaxBufferedSeconds", StringValue("1600"));

	    sinkApp.Add(client.Install (ueNodes.Get(u)));
	*/
  }

  // sink = StaticCast<PacketSink> (sinkApp.Get (0));

  /* Start Applications */
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simTime));
  serverApp.Start (Seconds (1.0));

  // PCAP debugging
  // p2pHelp.EnablePcapAll("lte_wifi_plus_lwa_lwip");
  //p2pHelpXw.EnablePcapAll("lte_wifi_p2p");
  // csmaHelp.EnablePcapAll ("lte_wifi_csma", true);

  //wifiPhyHelp.EnablePcap ("AccessPoint", apDevices);
  //wifiPhyHelp.EnablePcap ("Station", staDevices);

  // stop the simulation after simTime seconds
  Simulator::Stop(Seconds(simTime));

  /* Enable Traces */
  fprintf(stderr, "Installing DASH Tracers on all clients\n");
  std::string DashTraceFile = outputDir + "report.csv";
  DASHPlayerTracer::Install(ueNodes, DashTraceFile);

  //AsciiTraceHelper ascii;
  //ipv4RoutingHelper.PrintRoutingTableAllAt(Seconds (1), ascii.CreateFileStream (outputDir + "hostRoutingTable.txt"), Time::S);

  Config::SetDefault("ns3::RadioBearerStatsCalculator::DlRlcOutputFilename", StringValue(outputDir + "DlRlcStats.txt"));
  Config::SetDefault("ns3::RadioBearerStatsCalculator::UlRlcOutputFilename" , StringValue(outputDir + "UlRlcStats.txt"));
  Config::SetDefault("ns3::RadioBearerStatsCalculator::DlPdcpOutputFilename", StringValue(outputDir + "DlPdcpStats.txt"));
  Config::SetDefault("ns3::RadioBearerStatsCalculator::UlPdcpOutputFilename", StringValue(outputDir + "UlPdcpStats.txt"));

  Config::SetDefault("ns3::PhyStatsCalculator::DlRsrpSinrFilename", StringValue(outputDir + "DlRsrpSinrStats.txt"));
  Config::SetDefault("ns3::PhyStatsCalculator::UlSinrFilename", StringValue(outputDir + "UlSinrStats.txt"));
  Config::SetDefault("ns3::PhyStatsCalculator::UlInterferenceFilename", StringValue(outputDir + "UlInterferenceStats.txt"));

  Config::SetDefault("ns3::MacStatsCalculator::DlOutputFilename", StringValue(outputDir + "DlMacStats.txt"));
  Config::SetDefault("ns3::MacStatsCalculator::UlOutputFilename", StringValue(outputDir + "UlMacStats.txt"));

  Config::SetDefault("ns3::PhyTxStatsCalculator::DlTxOutputFilename", StringValue(outputDir + "DlTxPhyStats.txt"));
  Config::SetDefault("ns3::PhyTxStatsCalculator::UlTxOutputFilename", StringValue(outputDir + "UlTxPhyStats.txt"));
  Config::SetDefault("ns3::PhyRxStatsCalculator::DlRxOutputFilename", StringValue(outputDir + "DlRxPhyStats.txt"));
  Config::SetDefault("ns3::PhyRxStatsCalculator::UlRxOutputFilename", StringValue(outputDir + "UlRxPhyStats.txt"));

  lteHelper->EnableTraces();

  Config::SetDefault ("ns3::ConfigStore::Filename", StringValue (outputDir + "lwa-config.xml"));
  Config::SetDefault ("ns3::ConfigStore::Mode", StringValue ("Save"));
  ConfigStore outputConfig;
  outputConfig.ConfigureAttributes ();

  //FlowMonitor
  Ptr<FlowMonitor> flowMon;
  FlowMonitorHelper flowMonHelper;
  flowMon = flowMonHelper.InstallAll();

  ns3::AnimationInterface *anim;
  anim = new AnimationInterface (outputDir + "animation.xml");
  anim->AddResource("");
  anim->SetBackgroundImage ("/home/sanjay/git/ns-3.30/netanim-bg-white.jpeg", -180, -80, 5, 8, 0.1);

  Simulator::Run ();

  flowMon->SetAttribute("DelayBinWidth", DoubleValue(0.01));
  flowMon->SetAttribute("JitterBinWidth", DoubleValue(0.01));
  flowMon->SetAttribute("PacketSizeBinWidth", DoubleValue(1));
  flowMon->CheckForLostPackets();
  flowMon->SerializeToXmlFile(outputDir + "FlowMonitor.xml", true, true);

  Simulator::Destroy ();

  NS_LOG_INFO ("-------- Program end! -------------------");
  return 0;
}

