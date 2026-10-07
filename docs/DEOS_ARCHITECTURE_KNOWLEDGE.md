# DEOS Firmware - Architectural Knowledge Base & Technical Context
*Bu dosya, yapay zeka (AI) asistanların önceki oturumlarda öğrenilen kritik sistem mimarisini, iletişim kurallarını ve geçmişte çözülen darboğazları hatırlaması için hazırlanmıştır.*

## 1. Network (Ağ) ve Broadcast Mimarisi
- **Global Broadcast (0xFF):** Sisteme bağlı tüm arayüzlere (CAN-FD, LoRa UART, Ethernet) gönderilir. Acil durumlar (E-STOP) ve State (Durum) değişiklikleri gibi tüm araçların ve yer istasyonlarının anında duyması gereken kritik komutlar için kullanılır.
- **CAN Broadcast (0xFE):** Sadece donanımsal CAN-FD hattına gönderilir. LoRa (Havadan iletişim) gibi dar bant genişliğine (2.4 kbps) sahip kanalları felç etmemek için, saniyede bir atılan `HEARTBEAT` gibi yüksek frekanslı rutin mesajlar **sadece** CAN Broadcast üzerinden yapılır.

## 2. LoRa ve UART Optimizasyonları (Zero-Latency)
- LoRa (Ebyte) haberleşmesi STM32 ile **115200 baud** üzerinden donanım kesmesiyle (Interrupt) konuşur. Havadaki aktarım (Air Rate) kirliliğe karşı **2.4 kbps** gibi yavaş bir hıza ayarlıdır.
- **RX (Alma) Mimarisi:** Kesinlikle `k_msleep` (polling) kullanılmaz! UART'a bayt geldiğinde donanım kesmesi (IRQ) `ring_buf_put` yapar ve anında `k_sem_give` tetikleyerek `rx_thread`'i sıfır gecikmeyle (zero-latency) uyandırır. Bu optimizasyon sayede ping süreleri mükemmel tutarlılığa ulaşmıştır.
- **TX (Gönderme) Mimarisi:** Asenkron `k_msgq` (Message Queue) ve ayrı bir `tx_thread` kullanılır. CAN veya LoRa tarafında bir iletişim kopukluğu / kablo çıkması yaşanırsa, `deos_tx: TX Queue full, dropping message` uyarısı atılır ancak sistem **asla kilitlenmez**.

## 3. Router Kuralları (Split Horizon ve Loopback)
- **Kural:** Yönlendirici (Router), bir paketi geldiği arayüze **kesinlikle geri yönlendirmez** (Loopback Koruması / Split Horizon).
- **Test Yanılgısı:** Eğer Yer Kontrol (0x02), Router üzerinden sistemin LoRa ağını test etmek istiyorsa, hedefe kendini (0x02) yazamaz. Eğer yazarsa, Router paketin LoRa'dan geldiğini ve hedefin de LoRa'da olduğunu görüp paketi düşürür (Loop engelleme).
- **Doğru Test:** Yer Kontrol (0x02), Ana Beyin'e (STM32 -> 0x03) PING atmalıdır. STM32 bu PING'i alır, işler ve 0x02'ye PONG olarak cevap üretip LoRa'dan geri gönderir.

## 4. Routing Policy (Yönlendirme Politikası)
`MainApp.cpp` içerisindeki `c_routing_policy` güncel adres atamaları:
- `DEOS_NODE_GROUND_CONTROL (0x02)` -> `DEOS_TRANSPORT_UART_LORA` üzerinden haberleşir. Python test araçları (Yer Kontrol) bu kimliği kullanır.
- `DEOS_NODE_DIAG_TOOL (0xF0)` -> `DEOS_TRANSPORT_ETH_TEXTUAL` üzerinden haberleşir.

## 5. Python Araçları (Tools)
- `lora_config/module_config.py`: Yalnızca Ebyte donanımının (M0=1, M1=1 modunda) Baudrate ve Kanal (Channel 23 vb.) ayarlarını yapmak içindir.
- `tools/lora_test_suite/`: Sistemin (GROUND_CONTROL - 0x02 kimliğiyle) test edildiği ana Python scriptleridir (`ping_sender.py`, `state_controller.py`, `listener_only.py`). Ajanlar test yaparken her zaman bu klasördeki scriptleri baz almalıdır.

> **AI İÇİN NOT:** Herhangi bir LoRa, UART, Kesme (Interrupt) veya Yönlendirme (Routing) mantığı yazacağınız zaman yukarıdaki asenkron ve semafor tabanlı zero-latency mimariyi bozmadığınızdan emin olun.
