#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_gpudev.h>
#include <cuda_runtime.h>
#include <gdrapi.h>
// #include "my_headers.h"

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

static const struct rte_eth_conf port_conf_default = {
    .rxmode = { .max_lro_pkt_size = RTE_ETHER_MAX_LEN }
};

static void
gpu_process_packets(struct rte_mbuf **pkts, uint16_t nb_pkts) {
    // Example GPU processing function
    // Here you can add your CUDA kernel calls to process packets
}

int
main(int argc, char **argv) {
    struct rte_mempool *mbuf_pool;
    uint16_t portid = 0;

    // Initialize the Environment Abstraction Layer (EAL)
    int ret = rte_eal_init(argc, argv);
    if (ret < 0) rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

    // Create a memory pool
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS,
        MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (mbuf_pool == NULL) rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    // Initialize the Ethernet device
    struct rte_eth_conf port_conf = port_conf_default;
    ret = rte_eth_dev_configure(portid, 1, 1, &port_conf);
    if (ret < 0) rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n", ret, portid);

    ret = rte_eth_rx_queue_setup(portid, 0, 128, rte_eth_dev_socket_id(portid), NULL, mbuf_pool);
    if (ret < 0) rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n", ret, portid);

    ret = rte_eth_tx_queue_setup(portid, 0, 128, rte_eth_dev_socket_id(portid), NULL);
    if (ret < 0) rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n", ret, portid);

    ret = rte_eth_dev_start(portid);
    if (ret < 0) rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n", ret, portid);

    // Main loop
    struct rte_mbuf *bufs[BURST_SIZE];
    while (1) {
        const uint16_t nb_rx = rte_eth_rx_burst(portid, 0, bufs, BURST_SIZE);
        if (nb_rx > 0) {
            gpu_process_packets(bufs, nb_rx);
            rte_eth_tx_burst(portid, 0, bufs, nb_rx);
        }
    }

    // Cleanup
    rte_eth_dev_stop(portid);
    rte_eth_dev_close(portid);
    rte_eal_cleanup();

    return 0;
}
