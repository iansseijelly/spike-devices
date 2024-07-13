#include "rosearb.h"

bool rose_arb_t::load(reg_t addr, size_t len, uint8_t* bytes){
    uint32_t r = 0;
    switch (addr) {
        case ROSE_RX_DATA_ADDR: r = read_rxfifo(); break;
        case ROSE_STATUS_ADDR:  r = get_status();  break;
        default: printf("[ROSEARB]FAULTY_LOAD -- ADDR=0x%lx LEN=%lu\n", addr, len); abort();
    } 
    memcpy(bytes, &r, len);
    return true;
}

bool rose_arb_t::store(reg_t addr, size_t len, const uint8_t* bytes){
    switch (addr) {
        case ROSE_TX_DATA_ADDR: write_txfifo(*bytes); return true;
        default: printf("[ROSEARB]FAULTY_STORE -- ADDR=0x%lx LEN=%lu\n", addr, len); abort();
    }
}

void rose_arb_t::tick(reg_t UNUSED rtc_ticks){
    // if any word is in TX FIFO, send it
    while (tx_fifo.size() > 0){
        uint8_t data = tx_fifo.front();
        net_write(sync_sockfd, &data, ROSE_WORD_SIZE);
        tx_fifo.pop();
    }
 
    int n;
    // if any packet is in socket, read it
    do {
        bzero(buf, ROSE_BUF_SIZE);
        n = net_read(sync_sockfd, buf, ROSE_WORD_SIZE);
        if (n > 0) {
            uint8_t data = buf[0];
            rx_fifo.push(data);
        }
    } while (n > 0);
}

int fdt_parse_rose_arb(const void *fdt, reg_t *base_addr,
			  const char *compatible) {
  int nodeoffset, len, rc;
  const fdt32_t *reg_p;

  nodeoffset = fdt_node_offset_by_compatible(fdt, -1, compatible);
  if (nodeoffset < 0)
    return nodeoffset;

  rc = fdt_get_node_addr_size(fdt, nodeoffset, base_addr, NULL, "reg");
  if (rc < 0 || !base_addr)
    return -ENODEV;

  return 0;
}

rose_arb_t* rose_arb_parse_from_fdt(const void* fdt, const sim_t* sim, reg_t* base, std::vector<std::string> sargs) {
  if (fdt_parse_rose_arb(fdt, base, "ucb-bar,RoseAdapter") == 0) {
    printf("Found rose at %lx\n", *base);
    return new rose_arb_t(sim->get_intctrl(), 0);
  }
  return NULL;
}

std::string rose_arb_generate_dts(const sim_t* sim, const std::vector<std::string>& args) {
	std::stringstream s;
  s << std::hex
    << "    RoseAdapter@" << ROSE_BASE_ADDR << " {\n"
       "      compatible = \"ucb-bar,RoseAdapter\";\n"
       "      interrupt-parent = <&PLIC>;\n"
       "      interrupts = <" << std::dec << ROSE_INT_ID;
  reg_t blkdevbs = ROSE_BASE_ADDR;
  reg_t blkdevsz = ROSE_SIZE;
  s << std::hex << ">;\n"
       "      reg = <0x" << (blkdevbs >> 32) << " 0x" << (blkdevbs & (uint32_t)-1) <<
                   " 0x" << (blkdevsz >> 32) << " 0x" << (blkdevsz & (uint32_t)-1) << ">;\n"
       "    };\n";
  return s.str();
}

REGISTER_DEVICE(rose_arb, rose_arb_parse_from_fdt, rose_arb_generate_dts);