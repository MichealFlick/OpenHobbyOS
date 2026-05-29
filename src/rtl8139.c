#include "pci.h"
#include "netdev.h"
#include "io.h"
#include "idt.h"
#include "memory.h"
#include "console.h"
#include "pic.h"
#include "string.h"

#define RTL_VENDOR 0x10EC
#define RTL_DEVICE 0x8139

#define RX_BUF_SIZE 32768
#define TX_BUF_SIZE 1536

/* Register offsets */
#define RTL_MAC0    0x00
#define RTL_MAC4    0x04
#define RTL_TSD0    0x10
#define RTL_TSAD0   0x20
#define RTL_RBSTART 0x30
#define RTL_CR      0x37
#define RTL_CAPR    0x38
#define RTL_CBR     0x3A
#define RTL_IMR     0x3C
#define RTL_ISR     0x3E
#define RTL_RCR     0x3C
#define RTL_TCR     0x40
#define RTL_CONFIG1 0x52

/* CR bits */
#define CR_RST  0x10
#define CR_RE   0x08
#define CR_TE   0x04

/* TSD bits */
#define TSD_OWN 0x00010000
#define TSD_TOK 0x00008000

/* ISR bits */
#define ISR_ROK 0x0001
#define ISR_TOK 0x0002
#define ISR_ERR 0x0080
#define ISR_SERR 0x4000

typedef struct {
    u16 io_base;
    u8 irq;
    u8 mac[6];
    u8 *rx_buf;
    u32 rx_buf_phys;
    u32 rx_offset;
    u8 tx_buf[TX_BUF_SIZE] __attribute__((aligned(4)));
    u32 tx_buf_phys;
    netdev_t *netdev;
    bool initialized;
} rtl8139_dev_t;

static rtl8139_dev_t rtl_dev;

static u32 virt_to_phys(void *virt) {
    return (u32)(uintptr_t)virt;
}

static void rtl8139_irq_handler(registers_t *regs) {
    (void)regs;
    u16 isr = inw(rtl_dev.io_base + RTL_ISR);
    if (!isr) return;

    if (isr & ISR_ROK) {
        u16 capr = inw(rtl_dev.io_base + RTL_CAPR);
        u16 cbr = inw(rtl_dev.io_base + RTL_CBR);
        while (capr != cbr) {
            u8 *pkt = rtl_dev.rx_buf + capr;
            u16 rx_hdr = *(volatile u16 *)pkt;
            u16 pkt_len = rx_hdr & 0x3FFF;
            u16 pkt_status = *(volatile u16 *)(pkt + 2);

            if (pkt_len == 0xFFF0 || pkt_len == 0) break;

            if (pkt_status & 0x0001) {
                u16 data_len = pkt_len - 4;
                u8 *data = pkt + 4;
                if (rtl_dev.netdev) {
                    netdev_rx_push(rtl_dev.netdev, data, data_len);
                }
            }

            capr += pkt_len + 4;
            capr = (capr + 3) & ~3;
            if (capr >= RX_BUF_SIZE) capr -= RX_BUF_SIZE;

            outw(rtl_dev.io_base + RTL_CAPR, capr - 16);
            cbr = inw(rtl_dev.io_base + RTL_CBR);
        }
    }

    outw(rtl_dev.io_base + RTL_ISR, isr);
}

static int rtl8139_netdev_send(netdev_t *dev, const u8 *packet, u16 length) {
    (void)dev;
    if (length > TX_BUF_SIZE) return -1;
    memcpy(rtl_dev.tx_buf, packet, length);
    while (inl(rtl_dev.io_base + RTL_TSD0) & TSD_OWN);
    outl(rtl_dev.io_base + RTL_TSAD0, rtl_dev.tx_buf_phys);
    outl(rtl_dev.io_base + RTL_TSD0, length | TSD_OWN);
    return 0;
}

static void rtl8139_netdev_rx_enable(netdev_t *dev, bool enable) {
    (void)dev;
    u8 cr = inb(rtl_dev.io_base + RTL_CR);
    if (enable) {
        outb(rtl_dev.io_base + RTL_CR, cr | CR_RE);
    } else {
        outb(rtl_dev.io_base + RTL_CR, cr & ~CR_RE);
    }
}

static const netdev_ops_t rtl8139_ops = {
    .send = rtl8139_netdev_send,
    .rx_enable = rtl8139_netdev_rx_enable,
};

static bool rtl8139_probe(const pci_device_t *pci) {
    rtl_dev.io_base = (u16)(pci->bar0 & 0xFFFC);
    rtl_dev.irq = pci->irq;

    u32 cmd = pci_config_read(pci->bus, pci->slot, pci->func, 0x04);
    cmd |= 0x05;
    cmd &= ~0x04;
    pci_config_write(pci->bus, pci->slot, pci->func, 0x04, cmd);

    outb(rtl_dev.io_base + RTL_CR, CR_RST);
    for (int i = 0; i < 1000; i++) {
        io_wait();
        if (!(inb(rtl_dev.io_base + RTL_CR) & CR_RST)) break;
    }

    for (int i = 0; i < 6; i++) {
        rtl_dev.mac[i] = inb(rtl_dev.io_base + RTL_MAC0 + i);
    }

    rtl_dev.rx_buf = kmalloc_aligned(RX_BUF_SIZE + 16, 16);
    if (!rtl_dev.rx_buf) {
        console_printf("[rtl8139] failed to allocate RX buffer\n");
        return false;
    }
    rtl_dev.rx_buf_phys = virt_to_phys(rtl_dev.rx_buf);
    if ((rtl_dev.rx_buf_phys & 0xFFFF) + RX_BUF_SIZE > 0x10000) {
        console_printf("[rtl8139] RX buffer crosses 64K boundary, reallocating\n");
        kfree(rtl_dev.rx_buf);
        rtl_dev.rx_buf = kmalloc_aligned(RX_BUF_SIZE + 0x10000, 0x10000);
        if (!rtl_dev.rx_buf) {
            console_printf("[rtl8139] failed to allocate 64K-aligned RX buffer\n");
            return false;
        }
        rtl_dev.rx_buf_phys = virt_to_phys(rtl_dev.rx_buf);
    }
    rtl_dev.rx_offset = 0;

    rtl_dev.tx_buf_phys = virt_to_phys(rtl_dev.tx_buf);

    outl(rtl_dev.io_base + RTL_RBSTART, rtl_dev.rx_buf_phys);
    outl(rtl_dev.io_base + RTL_RCR, 0x00004F60);
    outl(rtl_dev.io_base + RTL_TCR, 0x000000E0);
    outw(rtl_dev.io_base + RTL_IMR, ISR_ROK | ISR_TOK | ISR_ERR);

    outb(rtl_dev.io_base + RTL_CR, CR_RE | CR_TE);

    console_printf("[rtl8139] at io=%x irq=%u mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
                   rtl_dev.io_base, rtl_dev.irq,
                   rtl_dev.mac[0], rtl_dev.mac[1], rtl_dev.mac[2],
                   rtl_dev.mac[3], rtl_dev.mac[4], rtl_dev.mac[5]);

    rtl_dev.netdev = netdev_by_id(0);
    rtl_dev.initialized = true;
    return true;
}

void rtl8139_init(void) {
    memset(&rtl_dev, 0, sizeof(rtl_dev));

    const pci_device_t *pci = pci_find(RTL_VENDOR, RTL_DEVICE);
    if (!pci) {
        console_printf("[rtl8139] no device found\n");
        return;
    }

    if (!rtl8139_probe(pci)) {
        console_printf("[rtl8139] probe failed\n");
        return;
    }

    netdev_ops_t ops = rtl8139_ops;
    int id = netdev_register("eth0", rtl_dev.mac, &ops, &rtl_dev);
    if (id < 0) {
        console_printf("[rtl8139] netdev register failed\n");
        return;
    }

    irq_install_handler(rtl_dev.irq, rtl8139_irq_handler);
    pic_clear_mask(rtl_dev.irq);

    console_printf("[rtl8139] initialized\n");
}

void rtl8139_poll(void) {
    if (!rtl_dev.initialized) return;
    u16 isr = inw(rtl_dev.io_base + RTL_ISR);
    if (isr) {
        rtl8139_irq_handler(NULL);
    }
}
