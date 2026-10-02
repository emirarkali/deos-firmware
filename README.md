# DEOS Core Protocol Stack

![Zephyr RTOS](https://img.shields.io/badge/Zephyr-RTOS-blue)
![C17](https://img.shields.io/badge/C-17-orange)
![CAN-FD](https://img.shields.io/badge/Bus-CAN--FD-green)

DEOS Core, STM32 tabanlı (veya uyumlu diğer donanımlar üzerindeki) Zephyr RTOS node'ları arasında geliştirilmiş, tamamen **CAN-FD** tabanlı, yüksek performanslı ve deterministik bir iletişim kütüphanesidir.

Traction, Steering, Brake gibi fiziksel kontrol düğümleri (node'lar), doğrudan CAN frame'leri ile uğraşmak yerine DEOS Core'un sunduğu `deos_send` ve `deos_register_handler` gibi yüksek seviyeli, güvenli ve test edilmiş API'leri kullanırlar.

## 🌟 Temel Özellikler (Key Features)

- **Tamamen CAN-FD Tabanlı:** 60 byte'a kadar command-specific payload ve yüksek hız. (Classic CAN desteği veya fallback mekanizması yoktur, donanım desteklemiyorsa `ENOTSUP` döner). BRS (Bit Rate Switch) aktif olarak kullanılır; controller data-phase bitrate ayarı board/devicetree/Kconfig tarafından sağlanmalıdır.
- **Sıfır Dinamik Bellek (Zero-Allocation):** `malloc` veya `free` kullanılmaz. Tamamen RAM dostu, statik ve deterministik bellek yönetimi (MISRA C / safety-critical yaklaşımlarına uygun).
- **Hosted Local Nodes (Çoklu Düğüm Desteği):** Aynı fiziksel donanım (MCU) ve CAN arayüzü üzerinde, birbirinden tamamen izole (bağımsız sequence counter, handler ve fault tabloları) birden çok mantıksal (logical) DEOS Node barındırma yeteneği.
- **Node-Specific Fault Management:** Sistemdeki hataların tespiti, saklanması (latching), temizlenmesi ve diagnostik akışlar (`GET_FAULTS`, `CLEAR_FAULTS`) her bir mantıksal düğüm (local node) için tamamen izole ve otonom olarak yönetilir.
- **Donanımsal RX Filtreleme (Hardware RX Filters):** Yalnızca kayıtlı (registered) node'lara ve Broadcast'e gelen mesajların MCU'yu uyandırmasını sağlayan donanım destekli dinamik CAN filtreleri.
- **Otonom PING/PONG:** Ağdaki canlılığı kontrol etmek için uygulamanın (application) haberi olmadan arka planda (doğru source ID'ler kurgulanarak) otomatik PONG yanıtı üretir.
- **Ayrıştırılmış Mimari:** Application katmanı (iş mantığı) ile Core katmanı (haberleşme, serialization, fault handling) birbirinden kesin çizgilerle ayrılmıştır. Core hiçbir BMS-specific logic içermez.

## 🏗 Mimari Felsefe (Separation of Concerns)

- **Application:** "Motoru 45 derece döndürmek istiyorum" veya "Şu sensör değerini göndermeliyim."
- **DEOS Core:** "Bu veriyi CAN-FD üzerinden sequence, version ve endianness kurallarına göre nasıl paketlerim/açarım?"
- **Control Thread:** "PID algoritmasını uygulayarak fiziksel donanımı nasıl süreceğim?"

## 📡 29-bit CAN ID Formatı (Extended ID)

Tüm DEOS mesajları 29-bit Extended Identifier kullanır:

| Bit Range | Uzunluk | Açıklama |
| :--- | :--- | :--- |
| **28..26** | 3 bit | **Priority:** Mesaj önceliği (örn. URGENT, STATUS, LOG) |
| **25..22** | 4 bit | **Message Class:** Mesaj sınıfı (Command, Status, Diagnostic vs.) |
| **21..16** | 6 bit | **Service ID:** İlgili servis (Steering, Brake, Power vb.) |
| **15..8** | 8 bit | **Destination Node:** Hedef node ID (0xFF Broadcast) |
| **7..0** | 8 bit | **Source Node:** Kaynak node ID |

## 📦 DEOS CAN-FD Data Format (Payload Layout)

DEOS Core ortak veri başlığı (Common Data Header) 4 byte'tır.

| Byte Offset | Alan | Açıklama |
| :--- | :--- | :--- |
| **0** | Version | Protocol Version |
| **1** | Sequence | Mesaj sıra numarası |
| **2** | Command | Command ID |
| **3** | Length | Command-specific payload length |
| **4..N** | Payload | Command-specific Payload |

Geriye kalan fiziksel CAN-FD byte'ları **zero padding** (sıfır dolgusu) ile doldurulur.

> **Önemli:** `Length` alanı sadece geçerli command-specific payload uzunluğunu belirtir ve Version, Sequence, Command veya Length'in kendisini içermez. Maximum command-specific payload = **60 bytes**'tır.

## 🚀 Hızlı Başlangıç (Quick Start)

### 1. Sistemin Başlatılması (Initialization)

```c
#include <deos/deos.h>

void main(void) {
    /* 1. Core konfigürasyonu (Örn: Steering Node, Normal mod) */
    struct deos_config config = {
        .node_id = DEOS_NODE_STEERING,
        .can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus)),
        .router_enabled = false,
        .hosted_nodes_enabled = true /* Birden çok node barındırma aktif */
    };

    /* 2. Sistemi ilklendir */
    if (deos_init(&config) != 0) {
        LOG_ERR("DEOS Init failed!");
        return;
    }

    /* 3. (İsteğe Bağlı) Ek bir Local Node kayıt et (Örn: Brake) */
    deos_register_local_node(DEOS_NODE_BRAKE);

    /* 4. Gelen komutlar için Handler kayıt et (Application katmanına bağla) */
    deos_register_handler_for_node(
        DEOS_NODE_STEERING, /* Hangi node için? */
        DEOS_CLASS_COMMAND, 
        DEOS_SERVICE_STEERING,
        DEOS_CMD_STEERING_SET_TARGET_ANGLE,
        steering_handler, 
        NULL
    );

    /* 5. Arka plan TX/RX thread'lerini başlat */
    deos_start();

    /* 6. Dışarıya mesaj gönder (Bağımsız sequence counter ile Source ID otomatik olarak korunur) */
    uint16_t current_angle = 450; 
    deos_send_from_node(
        DEOS_NODE_STEERING, /* Source Node */
        DEOS_NODE_MAIN_STM32, /* Target Node */ 
        DEOS_PRIO_STATUS,
        DEOS_CLASS_STATUS, 
        DEOS_SERVICE_STEERING,
        DEOS_CMD_STEERING_GET_STATUS,
        &current_angle, 
        sizeof(current_angle)
    );
}
```

## 🛡 Fault Management (Hata Yönetimi)
Fault Management modülü, "Hosted Local Nodes" yeteneği ile tam entegre çalışır. Her bir fiziksel veya barındırılan (hosted) düğüm (node) kendi **bağımsız hata tablosuna (fault context)** sahiptir.

- **Raising:** `deos_fault_raise_for_node(DEOS_NODE_STEERING, DEOS_FAULT_RX_QUEUE_OVERFLOW, DEOS_FAULT_SEVERITY_ERROR)`
- **Latching:** `deos_fault_latch_for_node()` ile sabitlenen hatalar sadece `CLEAR_FAULTS` komutu ile temizlenebilir.
- **Diagnostik Streaming:** Diğer bir node'dan (Örn: Main MCU) `GET_FAULTS` komutu geldiğinde, DEOS Core mesajı hedeflenen Local Node'a yönlendirir ve o node'a ait hata tablosunu (hedef Node'un Source kimliği ile) bloklanmayan (non-blocking) `k_work_delayable` altyapısı ile karşıya aktarır. Bir node stream yaparken diğeri de eşzamanlı olarak stream yapabilir.
- **Legacy Desteği:** `deos_fault_raise(...)` gibi eski API'ler, doğrudan donanımsal primary node'a (`config.node_id`) yazmak üzere "wrapper" olarak yerinde bırakılmıştır.

## 🎛 Donanımsal RX Filtreleme (Hardware RX Filter)
Sistem iki farklı rolü destekler:
- **Normal Node (`router_enabled = false`):** Donanım (Hardware) seviyesinde yalnızca *kendi Node ID'sine* veya *Broadcast (0xFF)* adresine gelen frame'leri kabul edecek şekilde iki adet filtre (maskeleme) oluşturur. Geri kalan trafik CPU'yu yormadan (interrupt tetiklemeden) donanımsal olarak reddedilir.
- **Router Node (`router_enabled = true`):** Main MCU gibi ağları birbirine bağlayan cihazlar, promiscuous (geniş) maskeleme kullanarak tüm uzatılmış (Extended) DEOS paketlerini kabul eder ve iç yazılımsal dispatcher/router (yönlendirici) kurallarına göre analiz eder.

## 🧪 Testler (Unit Testing)
Proje, Zephyr ZTEST framework'ü kullanılarak yazılmış kapsamlı bir test takımına sahiptir. Testleri koşturmak için:
```bash
west build -b native_sim tests/protocol/ -t run
# veya gerçek donanımda
west build -b nucleo_f439zi tests/protocol/ -p always
```
*(Not: `native_sim` (eski adıyla `native_posix`) ortamı mock CAN sürücüsü ile ZTEST için kullanılabilir).*

## 📂 Dizin Yapısı (Directory Structure)
```
.
├── CMakeLists.txt
├── lib/
│   └── deos_core/
│       ├── CMakeLists.txt
│       ├── include/deos/
│       │   ├── deos.h          (Public API)
│       │   ├── deos_fault.h    (Fault Definitions)
│       │   ├── deos_icd.h      (Protocol ID/Bit Definitions)
│       │   └── deos_types.h    (Structs & Types)
│       └── src/
│           ├── deos_core.c     (Init & Lifecycle)
│           ├── deos_dispatch.c (Incoming Msg Routing)
│           ├── deos_fault.c    (Fault Registry & Streaming)
│           ├── deos_rx.c       (CAN RX Thread & HW Filters)
│           ├── deos_tx.c       (CAN TX API)
│           ├── deos_network.c  (Auto Ping/Pong)
│           ├── deos_codec.c    (Serialization)
│           └── deos_router.c   (Cross-network Routing)
├── src/
│   └── main.c                  (Example App Integration)
└── tests/
    └── protocol/               (ZTEST Unit Tests)
```

## 🚧 Mevcut Durum (Development Status)
- ✅ **Codec & Validation:** Tamamlandı.
- ✅ **Hosted Local Nodes Registry & Multi-Node Sequence/Dispatch:** Tamamlandı.
- ✅ **Handler Registration & Node-Aware Dispatching:** Tamamlandı.
- ✅ **Dynamic HW Filters & RX/TX Static Threads:** Tamamlandı.
- ✅ **Auto PING/PONG & Node-Aware Networking:** Tamamlandı.
- ✅ **Per-Node Fault Management & Diagnostic Streaming:** Tamamlandı.
- ⚠️ Geliştirme kartı `nucleo_f439zi` donanımsal olarak CAN-FD desteklememektedir, API CAN-FD kısıtlamasını denetlediği için test/uyarlama senaryolarında donanım kısıtlamalarına dikkat edilmelidir.
- 🚧 (PROVISIONAL) Yönlendirme (Routing) mantığının farklı fiziksel ağlara (Ethernet vs.) aktarılmasına dair paket yapısı (framing) ICD üzerinde henüz son halini ("frozen") almadığından stub olarak bırakılmıştır.
# deos-firmware
