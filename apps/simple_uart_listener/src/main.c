#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(simple_uart, LOG_LEVEL_INF);

const struct device *uart_dev = DEVICE_DT_GET(DT_ALIAS(lora_uart));
const struct device *gpioe_dev;

static void uart_rx_cb(const struct device *dev, void *user_data)
{
    uint8_t c;
    int aux_val = -1;

    /* AUX pin durumunu oku (D4 = PE14) */
    if (gpioe_dev) {
        aux_val = gpio_pin_get(gpioe_dev, 14);
    }

    uart_irq_update(dev);

    if (uart_irq_rx_ready(dev)) {
        while (uart_fifo_read(dev, &c, 1) == 1) {
            LOG_INF("RX Byte: 0x%02X | AUX (D4): %d", c, aux_val);
        }
    }

    int err = uart_err_check(dev);
    if (err) {
        LOG_ERR("UART Error: %d", err);
    }
}

int main(void)
{
    LOG_INF("Simple UART Listener started");

    gpioe_dev = DEVICE_DT_GET(DT_NODELABEL(gpioe));
    if (device_is_ready(gpioe_dev)) {
        int err1 = gpio_pin_configure(gpioe_dev, 13, GPIO_OUTPUT_INACTIVE); /* M0 (D3) -> LOW */
        int err2 = gpio_pin_configure(gpioe_dev, 14, GPIO_INPUT);           /* AUX (D4) -> INPUT */
        LOG_INF("M0 (D3) err: %d, AUX (D4) ayarlandi err: %d", err1, err2);
    } else {
        LOG_WRN("gpioe not ready");
    }

    if (!device_is_ready(uart_dev)) {
        LOG_ERR("UART device not ready");
        return 0;
    }

    uart_irq_callback_user_data_set(uart_dev, uart_rx_cb, NULL);
    uart_irq_rx_enable(uart_dev);
    uart_irq_err_enable(uart_dev);

    LOG_INF("Listening on lpuart1...");

    while (1) {
        k_msleep(1000);
    }

    return 0;
}
