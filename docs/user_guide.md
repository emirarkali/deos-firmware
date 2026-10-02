# DEOS Core - Detaylı Kullanım Kılavuzu (User Guide)

Bu kılavuz, DEOS Core iletişim yığınını (protocol stack) Zephyr RTOS ortamında bir MCU üzerinde nasıl kuracağınızı, çoklu mantıksal düğümleri (Hosted Local Nodes) nasıl yöneteceğinizi ve ağ üzerinden nasıl haberleşeceğinizi adım adım anlatır.

---

## 1. Sisteme Genel Bakış ve Terminoloji

DEOS Core, dağıtık gömülü sistemler için CAN-FD üzerinden çalışan, statik bellek tahsisi (zero-allocation) kullanan, deterministik bir mesajlaşma katmanıdır.

- **Primary Node:** Donanımın (Örn: STM32) ağdaki ana ve fiziksel kimliğidir. Sistem başlatılırken (init) atanır.
- **Hosted Local Node:** Aynı fiziksel mikrodenetleyici üzerinde koşan, kendi mesaj sıra numarası (sequence), hata tablosu (fault table) ve mesaj işleyicileri (handlers) olan sanal/ekstra kimliklerdir.
- **Message Class & Service:** Uygulamanın alanlarını belirler (Örn: `DEOS_CLASS_COMMAND`, `DEOS_SERVICE_STEERING`).
- **Handler:** Gelen bir CAN paketini (hedefi bu MCU ise) yakalayan C callback fonksiyonudur.

---

## 2. Sistemin Başlatılması (Initialization)

DEOS Core'u kullanabilmek için öncelikle Zephyr'in donanım ağacından (Devicetree) CAN cihazını almalı ve `deos_init()` çağırmalısınız.

### Temel (Tekil Node) Başlatma

```c
#include <deos/deos.h>
#include <zephyr/logging/log.h>

void main(void)
{
    /* 1. Konfigürasyonu ayarla */
    struct deos_config config = {
        .node_id = DEOS_NODE_STEERING,                      /* MCU'nun ana kimliği */
        .can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus)), /* Zephyr CAN cihazı */
        .router_enabled = false,                            /* Yönlendirici mod kapalı */
        .hosted_nodes_enabled = false                       /* Sadece Primary Node çalışsın */
    };

    /* 2. Sistemi başlat */
    if (deos_init(&config) != 0) {
        LOG_ERR("DEOS Core baslatilamadi!");
        return;
    }

    /* 3. Arka plan TX/RX thread'lerini başlat */
    deos_start();
}
```

---

## 3. Çoklu Düğüm Barındırma (Hosted Local Nodes)

Bir MCU üzerinde (Örneğin Ana Kontrolcü - Main STM32) BMS veya Dashboard gibi başka lojik düğümleri barındırmak istiyorsanız `hosted_nodes_enabled = true` yapmalı ve başlatma adımları arasına `deos_register_local_node` eklemelisiniz.

```c
    struct deos_config config = {
        .node_id = DEOS_NODE_MAIN_STM32,
        .can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus)),
        .router_enabled = false,
        .hosted_nodes_enabled = true /* Ekstra node eklenecek! */
    };

    deos_init(&config);

    /* Ekstra node'ları kaydet (Maksimum 4 node desteklenir) */
    deos_register_local_node(DEOS_NODE_BMS_MAIN);
    deos_register_local_node(DEOS_NODE_DASHBOARD);

    deos_start(); // Start çağrıldığında HW filtreleri tüm bu node'lar için aktif edilir.
```

---

## 4. Mesaj İşleyicilerini (Handlers) Kaydetme

Bir komut veya durum mesajı geldiğinde uygulamanızın tetiklenmesi için bir callback fonksiyonu (handler) kaydetmelisiniz.

### Handler Fonksiyonu Tanımlama
```c
static void my_steering_cmd_handler(const deos_message_t *msg, void *user_data)
{
    /* Gelen payload'u okuma (Endianness kurallarına göre) */
    uint16_t target_angle = deos_get_u16_le(&msg->payload[0]);
    
    printk("Yeni Direksiyon Acisi Geldi: %d (Gonderen: 0x%02X)\n", target_angle, msg->source);
}
```

### Handler'ı Sisteme Tanıtma
**Önemli:** Handler kaydedilirken hangi mantıksal düğüme (`local_node`) geldiğinde tetikleneceğini belirtmelisiniz.

```c
    deos_register_handler_for_node(
        DEOS_NODE_STEERING,                 /* Hangi node adresine gelirse tetiklensin? */
        DEOS_CLASS_COMMAND,                 /* Mesaj Sınıfı */
        DEOS_SERVICE_STEERING,              /* Servis ID */
        DEOS_CMD_STEERING_SET_TARGET_ANGLE, /* Komut ID */
        my_steering_cmd_handler,            /* Callback */
        NULL                                /* Opsiyonel User Data */
    );
```
*(Not: Sadece Primary node kullanıyorsanız eski uyumlu `deos_register_handler(...)` fonksiyonunu kullanabilirsiniz.)*

---

## 5. Mesaj Gönderme

Ağdaki başka bir node'a veri veya komut göndermek için kullanılır. Mesajlar CAN-FD sınırları ve DEOS header'ı gereği maksimum **60 byte** command-specific payload içerebilir.

### Herhangi Bir Local Node Üzerinden Mesaj Gönderme

```c
    uint8_t payload[2];
    deos_put_u16_le(payload, 450); /* 45.0 derece - Little Endian kodlama */

    int ret = deos_send_from_node(
        DEOS_NODE_STEERING,       /* Gönderici (Source) - Mutlaka bu MCU'ya kayıtlı bir node olmalı */
        DEOS_NODE_MAIN_STM32,     /* Alıcı (Destination) */
        DEOS_PRIO_CONTROL,        /* Öncelik */
        DEOS_CLASS_STATUS,        /* Mesaj Sınıfı */
        DEOS_SERVICE_STEERING,    /* Servis */
        DEOS_CMD_STEERING_GET_STATUS, /* Mesaj tipi/komutu */
        payload,                  /* Veri */
        sizeof(payload)           /* Veri uzunluğu */
    );

    if (ret != 0) {
        printk("Mesaj gonderilemedi! Hata kodu: %d\n", ret);
    }
```
*(Not: Primary node'dan atıyorsanız wrapper fonksiyon olan `deos_send(...)` kullanabilirsiniz. O otomatik olarak Source = Primary Node yapacaktır.)*

---

## 6. Hata Yönetimi (Fault Management)

Uygulamanız sırasında donanımsal veya mantıksal bir hata tespit ettiğinizde (Örn: Sensör koptu, limit aşıldı), bunu DEOS Core'a bildirmelisiniz. DEOS Core bunu hafızaya alır ve diğer node'ların `GET_FAULTS` isteğine otonom olarak yanıt verir.

### Hata Fırlatma (Raising a Fault)

```c
    /* Fren Node'u için bir hata fırlatıyoruz */
    deos_fault_raise_for_node(
        DEOS_NODE_BRAKE, 
        DEOS_FAULT_BRAKE_SENSOR_DISCONNECTED, /* Uygulamanızın belirlediği hata kodu */
        DEOS_FAULT_SEVERITY_ERROR             /* Hatanın ciddiyeti (INFO, WARNING, ERROR, CRITICAL) */
    );
```

### Hatayı Giderme / Kitleme (Inactive / Latching)
Hata ortadan kalktığında Inactive durumuna çekilebilir. 
```c
    deos_fault_set_inactive_for_node(DEOS_NODE_BRAKE, DEOS_FAULT_BRAKE_SENSOR_DISCONNECTED);
```
Eğer hatanın kritik olduğunu ve kesinlikle Main MCU tarafından "Clear" gönderilene kadar kalmasını istiyorsanız:
```c
    deos_fault_latch_for_node(DEOS_NODE_BRAKE, DEOS_FAULT_BATTERY_OVERHEATED);
```

*(Not: Bu API'ler de tıpkı diğerleri gibi sadece `config.node_id` üzerinde işlem yapmak üzere `deos_fault_raise(...)` şeklindeki wrapper'lara sahiptir).*

---

## 7. Heartbeat Yönetimi (Application Level Scheduling)

DEOS Core içerisinde bir **HEARTBEAT** mesajlaşma desteği bulunur (`deos_send_heartbeat`). Ancak DEOS Core **otomatik bir arka plan timer'ı (periyodik gönderim) çalıştırmaz**. Heartbeat gönderim periyodu, "timeout" kararları, "node alive" gibi mantıksal denetimler ve hata fırlatma politikaları tamamen uygulamanın (Application) sorumluluğundadır.

### Heartbeat Gönderme (Örnek Periyodik Timer)
Zephyr `k_work` kullanarak kendi uygulamanızda şu şekilde heartbeat gönderimini yönetebilirsiniz (Önerilen periyot: 1 saniye):

```c
static struct k_work_delayable heartbeat_work;

static void heartbeat_work_fn(struct k_work *work)
{
    /* Primary node kimliğiyle broadcast gönderir */
    deos_send_heartbeat();

    /* Hosted bir node adına göndermek isterseniz:
       deos_send_heartbeat_from_node(DEOS_NODE_BMS_MAIN);
       deos_send_heartbeat_from_node(DEOS_NODE_BMS_AUX); */

    k_work_reschedule(&heartbeat_work, K_SECONDS(1));
}

void app_start(void)
{
    k_work_init_delayable(&heartbeat_work, heartbeat_work_fn);
    k_work_schedule(&heartbeat_work, K_SECONDS(1));
}
```

### Heartbeat Mesajlarını Yakalama (Receive)
PING/PONG döngüsünün aksine, gelen HEARTBEAT mesajları Core tarafından otomatik (sessiz) tüketilmez ve PONG üretmez. Diğer node'lardan gelen heartbeat'leri dinlemek için normal handler kaydınızı yapmalısınız:

```c
deos_register_handler(
    DEOS_CLASS_NETWORK,
    DEOS_SERVICE_SYSTEM,
    DEOS_CMD_SYSTEM_HEARTBEAT,
    my_heartbeat_handler, /* Uygulamanız timeout / alive logic'ini burada çalıştırır */
    NULL);
```

---

## 8. Otonom Özellikler (Sizin Kod Yazmanıza Gerek Olmayan Kısımlar)

Siz sistemi `deos_start()` ile başlattıktan sonra DEOS Core aşağıdaki işlemleri **arka planda kendi kendine** yapar:

1. **PING / PONG Yanıtı:** Başka bir cihaz bu MCU'daki (Primary veya Ekstra Local fark etmeksizin) herhangi bir Node ID'sine `PING` komutu atarsa, DEOS Core saniyesinde otomatik olarak `PONG` yanıtı üretir ve Source adresini kurgular.
2. **GET_FAULTS Akışı:** Bir cihaz `GET_FAULTS` mesajı gönderdiğinde, hedeflenen Local Node'un hata tablosu arka plan iş parçacıkları (k_work_delayable) ile yavaş yavaş (veri yolunu tıkamadan 100ms aralıklarla) karşı tarafa iletilir.
3. **Donanım Filtreleri (HW RX Filters):** Kayıt etmediğiniz (ve Broadcast olmayan) hiçbir mesaj CPU'yu uyandırmaz. Filtreler `deos_start` çağrıldığında CAN kontrolcüsüne gömülür.

---

## 9. Sık Karşılaşılan Hata Kodları

- `-ENOTSUP`: Çalıştığınız mikrodenetleyicinin CAN cihazı CAN-FD modunu desteklemiyor demektir. Devicetree veya Kconfig ayarlarınızı kontrol edin.
- `-EPERM`: `deos_send_from_node` kullanırken kendi üzerinizde barındırmadığınız (Kayıtsız) bir Source ID kullanarak başkasının kimliğine (spoofing) bürünmeye çalıştınız.
- `-EBUSY`: Sistem `deos_start()` ile başladıktan sonra yeni bir local node eklemeye veya dispatcher/router kuralı girmeye çalıştınız.
- `-ENOSPC`: Yaratabileceğiniz maksimum local node kapasitesine (varsayılan: 4) veya fault kayıt limitine (varsayılan: 32) ulaştınız.

---

## İyi Çalışmalar! 🚀
DEOS Core, safety-critical alanlarda MISRA C ilkelerine riayet ederek tasarlanmıştır. Tüm application (uygulama) logic'inizi `main.c` veya kendi application thread'lerinizde tutmalı, iletişim için sadece DEOS public API'lerine dokunmalısınız.
