/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015, IMDEA Networks Institute
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
 * Author: Hany Assasa <hany.assasa@gmail.com>
.*
 * This is a simple example to test TCP over 802.11n (with MPDU aggregation enabled).
 *
 * Network topology:
 *
 *   Ap    STA
 *   *      *
 *   |      |
 *   n1     n2
 *
 * In this example, an HT station sends TCP packets to the access point.
 * We report the total throughput received during a window of 100ms.
 * The user can specify the application data rate and choose the variant
 * of TCP i.e. congestion control algorithm to use.
 */

#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/string.h"
#include "ns3/log.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/mobility-model.h"
#include "ns3/packet-sink.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/tcp-westwood.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/AMuSt-module.h"
#include "ns3/netanim-module.h"

NS_LOG_COMPONENT_DEFINE ("WifiDashSimulationExample");

using namespace std;
using namespace ns3;

Ptr<PacketSink> sink;                         /* Pointer to the packet sink application */
uint64_t lastTotalRx = 0;                     /* The value of the last total received bytes */

void
CalculateThroughput ()
{
  Time now = Simulator::Now ();                                         /* Return the simulator's virtual time. */
  double cur = (sink->GetTotalRx () - lastTotalRx) * (double) 8 / 1e5;     /* Convert Application RX Packets to MBits. */
  std::cout << now.GetSeconds () << "s: \t" << cur << " Mbit/s" << std::endl;
  lastTotalRx = sink->GetTotalRx ();
  Simulator::Schedule (MilliSeconds (100), &CalculateThroughput);
}

std::string GetCurrentWorkingDir( void ) {
  char buff[250];
  char* cwd = getcwd( buff, 250 );
  std::cout << cwd;
  std::string current_working_dir(buff);
  return current_working_dir;
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("WifiDashSimulationExample", LOG_LEVEL_ALL);
  LogComponentDisable ("DASHFakeServerApplication", LOG_LEVEL_ALL);
  LogComponentDisable ("HttpServerApplication", LOG_LEVEL_ALL);
  LogComponentEnable ("ns3.DASHPlayerTracer", LOG_LEVEL_ALL);
  LogComponentDisable ("MultimediaConsumer", LOG_LEVEL_ALL);

  uint32_t payloadSize = 1472;                       /* Transport layer payload size in bytes. */
  std::string dataRate = "100Mbps";                  /* Application layer datarate. */
  std::string tcpVariant = "TcpNewReno";             /* TCP variant type. */
  std::string phyRate = "HtMcs7";                    /* Physical layer bitrate. */
  double simulationTime = 10;                        /* Simulation time in seconds. */
  bool pcapTracing = false;                          /* PCAP Tracing is enabled or not. */
  std::string DashTraceFile = "report.csv";
  std::string ServerThroughputTraceFile = "server_throughput.csv";
  std::string RepresentationType = "netflix";

  /* Command line argument parser setup. */
  CommandLine cmd;
  cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue ("dataRate", "Application data ate", dataRate);
  cmd.AddValue ("tcpVariant", "Transport protocol to use: TcpNewReno, "
                "TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpVeno, "
                "TcpBic, TcpYeah, TcpIllinois, TcpWestwood, TcpWestwoodPlus, TcpLedbat ", tcpVariant);
  cmd.AddValue ("phyRate", "Physical layer bitrate", phyRate);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("pcap", "Enable/disable PCAP Tracing", pcapTracing);
  cmd.Parse (argc, argv);

  tcpVariant = std::string ("ns3::") + tcpVariant;
  // Select TCP variant
  if (tcpVariant.compare ("ns3::TcpWestwoodPlus") == 0)
    {
      // TcpWestwoodPlus is not an actual TypeId name; we need TcpWestwood here
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
      // the default protocol type in ns3::TcpWestwood is WESTWOOD
      Config::SetDefault ("ns3::TcpWestwood::ProtocolType", EnumValue (TcpWestwood::WESTWOODPLUS));
    }
  else
    {
      TypeId tcpTid;
      NS_ABORT_MSG_UNLESS (TypeId::LookupByNameFailSafe (tcpVariant, &tcpTid), "TypeId " << tcpVariant << " not found");
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TypeId::LookupByName (tcpVariant)));
    }

  /* Configure TCP Options */
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (payloadSize));

  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));

  WifiMacHelper wifiMac;
  WifiHelper wifiHelper;
  wifiHelper.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);

  /* Set up Legacy Channel */
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel", "Frequency", DoubleValue (5e9));

  /* Setup Physical Layer */
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  wifiPhy.SetErrorRateModel ("ns3::YansErrorRateModel");
  wifiHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode", StringValue (phyRate),
                                      "ControlMode", StringValue ("HtMcs0"));

  NodeContainer networkNodes;
  networkNodes.Create (3);
  Ptr<Node> apWifiNode = networkNodes.Get (0);
  Ptr<Node> staWifiNode = networkNodes.Get (1);
  Ptr<Node> remoteHost = networkNodes. Get(2);

  /* Configure AP */
  Ssid ssid = Ssid ("network");
  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevice;
  apDevice = wifiHelper.Install (wifiPhy, wifiMac, apWifiNode);

  /* Configure STA */
  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid));

  NetDeviceContainer staDevices;
  staDevices = wifiHelper.Install (wifiPhy, wifiMac, staWifiNode);

  NetDeviceContainer p2pDevices = p2ph.Install (apWifiNode, remoteHost);


  /* Mobility model */
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (1.0, 1.0, 0.0));

  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apWifiNode);
  mobility.Install (staWifiNode);

  /* Internet stack */
  InternetStackHelper stack;
  stack.Install (networkNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign (apDevice);
  Ipv4InterfaceContainer staInterface;
  staInterface = address.Assign (staDevices);

  address.SetBase("11.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterface;
  p2pInterface = address.Assign(p2pDevices);

  /* Populate routing table */
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  /* Install TCP Receiver on the access point */

  /* Install TCP/UDP Transmitter on the station */
  uint16_t port = 80;
  std::string representationStrings = GetCurrentWorkingDir() + "/../content/segments/BigBuckBunny/bunny_2s/dash_dataset_avc_bbb.csv";
  fprintf(stderr, "representations = %s\n", representationStrings.c_str());
  DASHServerHelper server(p2pInterface.GetAddress (1), port,  "11.0.0.2", "/content/segments/BigBuckBunny/bunny_2s/", representationStrings, "/content/segments/BigBuckBunny/bunny_2s/");
  ApplicationContainer serverApp = server.Install (remoteHost);

  int screenWidth = 1240;
  int screenHeight = 1080;
  std::string AdaptationLogicToUse = "RateBasedAdaptationLogic"; // DASHJSAdaptationLogic
  AdaptationLogicToUse = "dash::player::" + AdaptationLogicToUse;

  std::stringstream ssMPDURL;
  ssMPDURL << "http://" << p2pInterface.GetAddress (1) << "/content/segments/BigBuckBunny/bunny_2s/vid1.mpd.gz";
  // NS_LOG_DEBUG(">>>>>>>>>>> add: " << ssMPDURL.str());
  DASHHttpClientHelper client(ssMPDURL.str());
  client.SetAttribute("AdaptationLogic", StringValue(AdaptationLogicToUse));
  client.SetAttribute("StartUpDelay", StringValue("0.5"));
  client.SetAttribute("ScreenWidth", UintegerValue(screenWidth));
  client.SetAttribute("ScreenHeight", UintegerValue(screenHeight));
  client.SetAttribute("AllowDownscale", BooleanValue(true));
  client.SetAttribute("AllowUpscale", BooleanValue(true));
  client.SetAttribute("MaxBufferedSeconds", StringValue("1600"));

  ApplicationContainer sinkApp = client.Install (staWifiNode);
  // sink = StaticCast<PacketSink> (sinkApp.Get (0));

  /* Start Applications */
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simulationTime));
  serverApp.Start (Seconds (1.0));
  // serverApp.Stop (Seconds (simulationTime));
  // Simulator::Schedule (Seconds (1.1), &CalculateThroughput);

  /* Enable Traces */
  fprintf(stderr, "Installing DASH Tracers on all clients\n");
  DASHPlayerTracer::Install(staWifiNode, DashTraceFile);

  fprintf(stderr, "Installing one NodeThroughputTracer\n");
  NodeThroughputTracer::Install(apWifiNode, ServerThroughputTraceFile);

  //AsciiTraceHelper ascii;
  //Ipv4RoutingHelper::PrintRoutingTableAllEvery(Seconds(5), ascii.CreateFileStream ("hostRoutingTable.txt"), Time::S);

  if (pcapTracing)
    {
      wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
      wifiPhy.EnablePcap ("AccessPoint", apDevice);
      wifiPhy.EnablePcap ("Station", staDevices);
    }

  ns3::AnimationInterface *anim;
  anim = new AnimationInterface ("animation.xml");
  anim->AddResource("");

  /* Start Simulation */
  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
