#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/ring_buffer.h>
#include <string.h>

#define RING_BUF_SIZE 1024
RING_BUF_DECLARE(uart_rb, RING_BUF_SIZE);

static volatile uint32_t rx_interrupt_count = 0;
static volatile uint32_t dropped_byte_count = 0;

static void uart_rx_cb(const struct device *dev, void *user_data)
{
    uint8_t c;

    // Update IRQ status
    uart_irq_update(dev);

    // Check if RX data is ready
    if (uart_irq_rx_ready(dev)) {
        rx_interrupt_count++;
        
        // Read until FIFO is empty
        while (uart_fifo_read(dev, &c, 1) == 1) {
            if (ring_buf_put(&uart_rb, &c, 1) == 0) {
                // Buffer overflow
                dropped_byte_count++;
            }
        }
    }
}

#include <zephyr/drivers/gpio.h>

int main(void)
{
    const struct device *uart_dev = DEVICE_DT_GET(DT_ALIAS(lora_uart));
    const struct device *gpiog = DEVICE_DT_GET(DT_NODELABEL(gpiog));
    const struct device *gpioe = DEVICE_DT_GET(DT_NODELABEL(gpioe));

    if (!device_is_ready(uart_dev)) {
        printk("\n[FATAL] lora-uart cihazi hazir degil!\n");
        return 0;
    }

    if (device_is_ready(gpiog)) {
        gpio_pin_configure(gpiog, 14, GPIO_OUTPUT_INACTIVE); // M0 (D2) -> LOW
    } else {
        printk("[UYARI] gpiog hazir degil!\n");
    }

    if (device_is_ready(gpioe)) {
        gpio_pin_configure(gpioe, 13, GPIO_OUTPUT_INACTIVE); // M1 (D3) -> LOW
    } else {
        printk("[UYARI] gpioe hazir degil!\n");
    }

    printk("\n=======================================================\n");
    printk(" NUCLEO-H563ZI UART TEST & LORA LISTENER \n");
    printk("=======================================================\n\n");

    // Configure UART interrupt using the modern API
    uart_irq_callback_user_data_set(uart_dev, uart_rx_cb, NULL);
    uart_irq_rx_enable(uart_dev);

    // --- A) USART LOOPBACK TESTI ---
    printk("A) LPUART1 (PB6-PB7) LOOPBACK TESTI BASLIYOR...\n");
    printk("Lutfen PB6 (TX) ve PB7 (RX) pinlerini KISA DEVRE YAPIN.\n");
    printk("Test verisi gonderiliyor: 'TEST_BYTE' (0xAA)\n\n");
    
    // Clear ring buffer just in case
    ring_buf_reset(&uart_rb);
    rx_interrupt_count = 0;
    dropped_byte_count = 0;

    // Send a known sequence
    uint8_t test_seq[] = {0xAA, 0xBB, 0xCC, 0xDD};
    for (int i = 0; i < sizeof(test_seq); i++) {
        uart_poll_out(uart_dev, test_seq[i]);
    }

    // Wait for RX interrupt to process the loopback data
    k_msleep(500);

    // Verify Loopback Data
    uint8_t rx_buf[16];
    uint32_t read_len = ring_buf_get(&uart_rb, rx_buf, sizeof(rx_buf));
    
    printk("--- Loopback Sonuclari ---\n");
    printk("Interrupt Tetiklenme Sayisi : %u\n", rx_interrupt_count);
    printk("Kuyruga Giren Byte Sayisi   : %u\n", read_len);
    
    if (read_len > 0) {
        printk("Alinan Veriler (HEX)        : ");
        for (int i = 0; i < read_len; i++) {
            printk("%02X ", rx_buf[i]);
        }
        printk("\n");
        
        bool success = true;
        for(int i = 0; i < sizeof(test_seq) && i < read_len; i++){
            if(rx_buf[i] != test_seq[i]){
                success = false;
            }
        }
        if (success && read_len == sizeof(test_seq)) {
            printk("\n[BASARILI] LPUART1 Kusursuz Calisiyor! RX/TX pinleri ve Interrupt saglam.\n");
        } else {
            printk("\n[UYARI] Veri alindi ama eksik/hatali! Pinctrl veya baud sorunu olabilir.\n");
        }
    } else {
        printk("\n[HATA] HIC VERI ALINAMADI!\n");
        printk("Olasiliklar:\n");
        printk("1. PB6 ve PB7 fiziksel olarak kisa devre degil.\n");
        printk("2. Pinler baska bir donanimla cakisiyor.\n");
        printk("3. Interrupt aktif edilemedi.\n");
    }

    printk("\n=======================================================\n");
    printk(" B) LORA E220 PING-PONG TESTI (Gecikme Olcumu) \n");
    printk(" PB6-PB7 kisa devresini sokup, E220 modülünü baglayin:\n");
    printk(" E220 TXD ---> PB7 (STM32 RX)\n");
    printk(" E220 RXD ---> PB6 (STM32 TX)\n");
    printk(" GND ---> Ortak GND\n");
    printk("=======================================================\n\n");
    
    // Clear buffer again
    ring_buf_reset(&uart_rb);

    uint32_t total_packets = 0;

    while (1) {
        char packet[64];
        uint32_t send_time = k_uptime_get_32();
        
        // Sadece PING <Sayi> gonderiyoruz
        int len = snprintf(packet, sizeof(packet), "PING %u\r\n", total_packets);

        printk("\n[LORA_TX] PING %u Gonderiliyor...\n", total_packets);
        for (int i = 0; i < len; i++) {
            uart_poll_out(uart_dev, packet[i]);
        }
        
        // PONG mesajini bekle
        uint32_t wait_start = k_uptime_get_32();
        char rx_buf[64];
        int rx_idx = 0;
        bool pong_received = false;
        
        // 3 saniye (3000ms) zaman asimi (LoRa RF hizi yavas olabilecegi icin uzun tuttuk)
        while (k_uptime_get_32() - wait_start < 3000) { 
            uint8_t c;
            if (ring_buf_get(&uart_rb, &c, 1) == 1) {
                if (c == '\n' || c == '\r') {
                    if (rx_idx > 0) {
                        rx_buf[rx_idx] = '\0';
                        if (strncmp(rx_buf, "PONG", 4) == 0) {
                            pong_received = true;
                            break;
                        }
                        rx_idx = 0;
                    }
                } else {
                    if (rx_idx < sizeof(rx_buf) - 1) {
                        rx_buf[rx_idx++] = (char)c;
                    }
                }
            } else {
                k_msleep(1); // Modulu ve islemciyi dinlendir
            }
        }
        
        if (pong_received) {
            uint32_t rtt = k_uptime_get_32() - send_time;
            printk("--> [BASARILI] %s alindi! Gecikme (RTT): %u ms\n", rx_buf, rtt);
        } else {
            printk("--> [ZAMAN ASIMI] Cevap alinamadi...\n");
        }
        
        total_packets++;
        
        // Diger ping'e gecmeden once bekleyelim (hem okunabilirlik hem stabilite icin)
        k_msleep(500); 
    }
    
    return 0;
}
