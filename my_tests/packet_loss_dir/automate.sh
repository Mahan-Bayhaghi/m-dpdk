#!/bin/bash

# Define ranges for the parameters
rx_rates=(250 500 1000 2000 5000)
rx_ring_sizes=(512 1024 2048 4096)
# processing_times=(0 10 25 50 100)  # in milliseconds
processing_times=(0)

# The pcap file and NIC interface for traffic generation
PCAP_FILE="./snake_pattern.pcap"
INTERFACE="enp114s0f1"

# Iterate over each combination of parameters
for rate in "${rx_rates[@]}"; do
  for ring_size in "${rx_ring_sizes[@]}"; do
    for proc_time in "${processing_times[@]}"; do
      touch config
      echo "${ring_size}" > config
      echo "${proc_time}" >> config

      echo "Testing rx_rate=${rate}, rx_ring_size=${ring_size}, processing_time=${proc_time}"
      # Run the DPDK packet loss program
      sudo ./packet_loss $ring_size $proc_time -- -l 0-1 -n 4 -- -p 0x2 &

      # Sleep to allow the program to start
      sleep 1.5

      # Send traffic at the specified rate
      sudo tcpreplay --mbps=$rate --intf1=$INTERFACE $PCAP_FILE > replay-config

      # Wait for a few seconds to ensure packets are received
      sleep 2

      # Kill the packet loss program
      sudo pkill packet_loss

      echo "Test complete"
      echo "---------------------------------------------"

      # Sleep for a bit before the next test
      # echo "" > config
      # rm config
      sleep 1
    done
  done
done

rm replay-config
