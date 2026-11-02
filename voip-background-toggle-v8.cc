/* SPDX-License-Identifier: GPL-2.0-only
 * Adapted from ns-3 simple-global-routing.cc for ns-3.45.
 * One UDP VoIP flow competes with a fixed-rate UDP background flow.
 * Run the same program with --qos=0 (FIFO) and --qos=1 (strict priority).
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

using namespace ns3;

static void
ReadSinkBytes(Ptr<PacketSink> sink, uint64_t* bytes)
{
    *bytes = sink->GetTotalRx();
}

int
main(int argc, char* argv[])
{
    bool qos = false;
    double backgroundMbps = 12.0;
    constexpr double duration = 30.0;
    constexpr uint32_t queuePackets = 100;
    std::string csvFile = "voip-qos-background-results.csv";

    CommandLine cmd(__FILE__);
    cmd.AddValue("qos", "0 = FIFO without QoS, 1 = strict VoIP priority", qos);
    cmd.AddValue("background", "UDP background offered rate in Mbit/s", backgroundMbps);
    cmd.AddValue("csv", "CSV file for this run's results", csvFile);
    cmd.Parse(argc, argv);

    NS_ABORT_MSG_IF(backgroundMbps <= 0.0,
                    "Background rate must be greater than 0 Mbit/s");

    const uint64_t backgroundBps =
        static_cast<uint64_t>(std::llround(backgroundMbps * 1e6));
    
    // Korrigerade tider för exakt 30 sekunder totalt utan fördröjning
    const double backgroundStart = 0.0;
    const double voipStart = 0.0; 
    const double trafficStop = duration;
    const double simulationStop = trafficStop + 2.0; // Kort dränering för sista paketen
    
    constexpr uint16_t voipPort = 5000;
    constexpr uint16_t backgroundPort = 9000;

    NodeContainer nodes;
    nodes.Create(4);
    InternetStackHelper internet;
    internet.Install(nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("50Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));
    p2p.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("100p"));
    NetDeviceContainer d0d2 = p2p.Install(NodeContainer(nodes.Get(0), nodes.Get(2)));
    NetDeviceContainer d1d2 = p2p.Install(NodeContainer(nodes.Get(1), nodes.Get(2)));

    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("25ms"));
    p2p.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("100p"));
    NetDeviceContainer d3d2 = p2p.Install(NodeContainer(nodes.Get(3), nodes.Get(2)));

    TrafficControlHelper tc;
    if (qos)
    {
        uint16_t root = tc.SetRootQueueDisc(
            "ns3::PrioQueueDisc",
            "Priomap",
            StringValue("1 1 1 1 0 1 1 1 1 1 1 1 1 1 1 1"));
        auto classes = tc.AddQueueDiscClasses(root, 2, "ns3::QueueDiscClass");
//        const uint32_t highPackets = (queuePackets + 1) / 2;
//        const uint32_t lowPackets = queuePackets / 2;
	const uint32_t highPackets = 2;
	const uint32_t lowPackets = queuePackets - highPackets;

        tc.AddChildQueueDisc(root, classes[0], "ns3::FifoQueueDisc",
                             "MaxSize", StringValue(std::to_string(highPackets) + "p"));
        tc.AddChildQueueDisc(root, classes[1], "ns3::FifoQueueDisc",
                             "MaxSize", StringValue(std::to_string(lowPackets) + "p"));
    }
    else
    {
        tc.SetRootQueueDisc("ns3::FifoQueueDisc",
                            "MaxSize", StringValue(std::to_string(queuePackets) + "p"));
    }
    tc.Install(d3d2.Get(1));

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = address.Assign(d0d2);
    address.SetBase("10.1.2.0", "255.255.255.0");
    address.Assign(d1d2);
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i3i2 = address.Assign(d3d2);

    TrafficControlHelper removeOtherQdiscs;
    removeOtherQdiscs.Uninstall(d0d2);
    removeOtherQdiscs.Uninstall(d1d2);
    removeOtherQdiscs.Uninstall(d3d2.Get(0));
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    const Ipv4Address receiver = i3i2.GetAddress(0);

    PacketSinkHelper voipSink("ns3::UdpSocketFactory",
                              InetSocketAddress(Ipv4Address::GetAny(), voipPort));
    ApplicationContainer voipReceiver = voipSink.Install(nodes.Get(3));
    voipReceiver.Start(Seconds(0));
    voipReceiver.Stop(Seconds(simulationStop));

    OnOffHelper voip("ns3::UdpSocketFactory",
                     InetSocketAddress(receiver, voipPort));
    voip.SetConstantRate(DataRate("64kbps"), 160);
    voip.SetAttribute("Tos", UintegerValue(0xb8));
    ApplicationContainer voipSender = voip.Install(nodes.Get(0));
    voipSender.Start(Seconds(voipStart));
    voipSender.Stop(Seconds(trafficStop));

    PacketSinkHelper backgroundSink("ns3::UdpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), backgroundPort));
    ApplicationContainer backgroundReceiver = backgroundSink.Install(nodes.Get(3));
    backgroundReceiver.Start(Seconds(0));
    backgroundReceiver.Stop(Seconds(simulationStop));
    Ptr<PacketSink> backgroundSinkPtr =
        DynamicCast<PacketSink>(backgroundReceiver.Get(0));

    OnOffHelper background("ns3::UdpSocketFactory",
                           InetSocketAddress(receiver, backgroundPort));
    background.SetConstantRate(DataRate(backgroundBps), 1200);
    background.SetAttribute("Tos", UintegerValue(0x00));
    ApplicationContainer backgroundSender = background.Install(nodes.Get(1));
    backgroundSender.Start(Seconds(backgroundStart));
    backgroundSender.Stop(Seconds(trafficStop));

    uint64_t backgroundBytesStart = 0;
    uint64_t backgroundBytesStop = 0;
    Simulator::Schedule(Seconds(voipStart), &ReadSinkBytes,
                        backgroundSinkPtr, &backgroundBytesStart);
    Simulator::Schedule(Seconds(trafficStop), &ReadSinkBytes,
                        backgroundSinkPtr, &backgroundBytesStop);

    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll();

    Simulator::Stop(Seconds(simulationStop));
    Simulator::Run();
    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    uint64_t txPackets = 0;
    uint64_t rxPackets = 0;
    double delaySeconds = 0.0;
    uint64_t backgroundTxPackets = 0;
    uint64_t backgroundRxPackets = 0;
    double backgroundDelaySeconds = 0.0;

    for (const auto& entry : monitor->GetFlowStats())
    {
        const auto tuple = classifier->FindFlow(entry.first);
        if (tuple.protocol == 17 &&
            tuple.sourceAddress == i0i2.GetAddress(0) &&
            tuple.destinationAddress == receiver &&
            tuple.destinationPort == voipPort)
        {
            txPackets += entry.second.txPackets;
            rxPackets += entry.second.rxPackets;
            delaySeconds += entry.second.delaySum.GetSeconds();
        }
        else if (tuple.protocol == 17 &&
                 tuple.destinationAddress == receiver &&
                 tuple.destinationPort == backgroundPort)
        {
            backgroundTxPackets += entry.second.txPackets;
            backgroundRxPackets += entry.second.rxPackets;
            backgroundDelaySeconds += entry.second.delaySum.GetSeconds();
        }
    }

    double voipLossPercent = 0.0;
    if (txPackets > 0)
    {
        voipLossPercent = 100.0 * (1.0 - (static_cast<double>(rxPackets) / txPackets));
    }
    double voipMeanDelayMs = 0.0;
    if (rxPackets > 0)
    {
        voipMeanDelayMs = (delaySeconds / rxPackets) * 1000.0;
    }

    double backgroundLossPercent = 0.0;
    if (backgroundTxPackets > 0)
    {
        backgroundLossPercent = 100.0 * (1.0 - (static_cast<double>(backgroundRxPackets) / backgroundTxPackets));
    }
    double backgroundMeanDelayMs = 0.0;
    if (backgroundRxPackets > 0)
    {
        backgroundMeanDelayMs = (backgroundDelaySeconds / backgroundRxPackets) * 1000.0;
    }

    uint64_t backgroundBytesWindow = backgroundBytesStop - backgroundBytesStart;
    double backgroundGoodputMbps = (backgroundBytesWindow * 8.0) / (duration * 1e6);

    std::cout << "\n========== Simulation results ==========\n";
    std::cout << "Queue mode: " << (qos ? "QoS ON - strict priority" : "QoS OFF - FIFO") << "\n";
    std::cout << "Background offered load: " << std::fixed << std::setprecision(3) << backgroundMbps << " Mbit/s UDP\n";
    std::cout << "Bottleneck capacity: 10 Mbit/s\n";
    std::cout << "Queue-disc budget: " << queuePackets << " packets total\n";
    std::cout << "VoIP measurement window: " << std::fixed << std::setprecision(3) << duration << " seconds\n";
    std::cout << "Background-flow statistics: full " << duration << "-second flow\n";
    std::cout << "\n--- VoIP flow ---\n";
    std::cout << "VoIP packets sent by source: " << txPackets << " packets\n";
    std::cout << "VoIP packets received at destination: " << rxPackets << " packets\n";
    std::cout << "VoIP packets not received: " << (txPackets - rxPackets) << " packets\n";
    std::cout << "VoIP packet loss rate: " << voipLossPercent << " % of sent packets\n";
    std::cout << "VoIP mean one-way delay: " << voipMeanDelayMs << " ms\n";
    std::cout << "\n--- UDP background flow ---\n";
    std::cout << "Background UDP packets sent by source: " << backgroundTxPackets << " packets\n";
    std::cout << "Background UDP packets received at destination: " << backgroundRxPackets << " packets\n";
    std::cout << "Background UDP packets not received: " << (backgroundTxPackets - backgroundRxPackets) << " packets\n";
    std::cout << "Background UDP packet loss rate: " << backgroundLossPercent << " %\n";
    std::cout << "Background UDP mean one-way delay: " << backgroundMeanDelayMs << " ms\n";
    std::cout << "Background UDP goodput: " << backgroundGoodputMbps << " Mbit/s\n";
    std::ifstream checkFile(csvFile);
    bool writeHeader = checkFile.peek() == std::ifstream::traits_type::eof();
    checkFile.close();
    std::ofstream csv(csvFile, std::ios_base::app);
    if (writeHeader)
    {
    csv << "qos,background_mbps,voip_mean_delay_ms,voip_loss_percent,background_loss_percent,background_mean_delay_ms,background_rx_goodput_mbps\n";
    }
    csv << (qos ? "ON" : "OFF") << ","
    << backgroundMbps << ","
    << voipMeanDelayMs << ","
    << voipLossPercent << ","
    << backgroundLossPercent << ","
    << backgroundMeanDelayMs << ","
    << backgroundGoodputMbps << "\n";
    csv.close();
    std::cout << "CSV results saved to: " << csvFile << "\n========================================\n";
    Simulator::Destroy();
    return 0;
}
