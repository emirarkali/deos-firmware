Bu Zephyr projesinde DEOS CAN-FD haberleşme protokolü için çalışan ve
birden fazla STM32 node tarafından ortak kullanılacak reusable bir
"DEOS Core" kütüphanesi geliştir.

Çalışma dizini:

~/zephyrproject/applications/deos_core_proto


Mevcut proje yapısı:

.
├── app.overlay
├── boards
├── board.txt
├── CMakeLists.txt
├── CMakeLists.txt.bak
├── lib
│   └── deos_core
│       ├── CMakeLists.txt
│       ├── include
│       │   └── deos
│       │       ├── deos.h
│       │       ├── deos_icd.h
│       │       └── deos_types.h
│       └── src
│           ├── deos_core.c
│           ├── deos_dispatch.c
│           ├── deos_rx.c
│           └── deos_tx.c
├── prj.conf
├── PROJECT_INFO.md
└── src
    └── main.c


============================================================
1. GENEL AMAÇ
============================================================

Amaç, DEOS sistemindeki STM32 tabanlı node'ların tamamında ortak kullanılacak
bir CAN-FD protocol stack / communication core oluşturmaktır.

Örnek node'lar:

- Main STM32
- Traction STM32
- Steering STM32
- Brake STM32
- BMS Gateway

DEOS Core application/control algoritmalarını çalıştırmayacak.

DEOS Core şu görevlerden sorumlu olacak:

- DEOS protocol registry
- DEOS protocol types
- CAN-FD 29-bit Extended Identifier encode/decode
- application payload encode/decode
- CAN-FD RX
- CAN-FD TX
- RX queue
- DEOS RX worker thread
- dispatcher
- common SYSTEM işlemleri
- NETWORK işlemleri
- PING / PONG
- HEARTBEAT altyapısı
- handler registration
- application message dispatch
- TX sequence yönetimi
- protocol validation
- local Node ID yönetimi
- Main STM32 routing altyapısı
- logging
- error handling

Aşağıdaki işler DEOS Core'un sorumluluğu DEĞİLDİR:

- PID
- motor control
- encoder processing
- steering control algorithm
- brake control algorithm
- traction control algorithm
- sensor fusion
- actuator control
- physical system state machine


============================================================
2. TEMEL MİMARİ PRENSİBİ
============================================================

DEOS Core bir "thread" değildir.

DEOS Core ortak bir library'dir.

Library içerisinde:

deos_init()
deos_send()
deos_receive/decode()
deos_dispatch()
deos_send_ping()
deos_send_pong()
handler registry
CAN codec
network logic

gibi fonksiyonlar bulunur.

Buna ek olarak DEOS Core kendi RX worker thread'ini çalıştırabilir.

Dolayısıyla kavramsal ayrım:

DEOS Core
    = library / communication protocol implementation

DEOS RX Thread
    = CAN RX queue'dan frame alıp DEOS Core fonksiyonlarını çalıştıran thread

Application Thread
    = PID / control / actuator / sensor gibi gerçek sistem işlerini yapan thread


Örnek:

CAN-FD RX callback
        |
        v
Static RX Queue
        |
        v
DEOS RX Thread
        |
        v
Frame Decode
        |
        v
DEOS Dispatcher
        |
        +---- Common DEOS behaviour
        |       |
        |       +---- PING/PONG
        |       +---- HEARTBEAT
        |
        +---- Application Handler
                |
                +---- Steering command
                +---- Brake command
                +---- Traction command


============================================================
3. CAN-FD ZORUNLULUĞU
============================================================

Bu proje TAMAMEN CAN-FD üzerine geliştirilecektir.

Classic CAN desteği İSTEMİYORUM.

Classic CAN için:

- fallback yazma
- 8 byte limitation ekleme
- compatibility mode oluşturma
- fragmentation oluşturma
- özel legacy transport oluşturma

Mevcut board:

nucleo_f439zi

CAN-FD desteklemiyor olabilir.

Bu önemli değil.

BOARD PROTOKOLE UYACAK.
PROTOKOL BOARD'A GÖRE DEĞİŞMEYECEK.

Eğer mevcut nucleo_f439zi board'u CAN-FD desteklemediği için build olmazsa:

- DEOS mimarisini Classic CAN'e çevirme
- CAN-FD kodunu kaldırma
- payload limitini 8 byte yapma

Sadece açıkça raporla:

"Current board does not provide CAN-FD/FDCAN capability."

Gerekirse daha sonra CAN-FD destekli STM32 board'a geçilecektir.

Build başarısı şu aşamada protokol tasarımından daha önemli değildir.


============================================================
4. CAN-FD TRANSPORT
============================================================

CAN transport Zephyr CAN API üzerinden yazılmalı.

Direct STM32 register programming yapma.

Zephyr CAN-FD desteğini kullan.

Kullanılan Zephyr sürümündeki gerçek API isimlerini header'lardan doğrula.

Tahmin ederek API üretme.

CAN-FD frame:

- Extended ID
- FDF
- mümkünse BRS configuration support

kullanılacak.

29-bit Extended Identifier zorunlu.

Maximum CAN-FD data field:

64 byte

DEOS application overhead:

3 byte

Dolayısıyla:

DEOS_MAX_FRAME_DATA_LEN = 64
DEOS_PROTOCOL_OVERHEAD  = 3
DEOS_MAX_PAYLOAD_LEN    = 61


============================================================
5. PROTOCOL VERSION
============================================================

DEOS Protocol v1.0:

0x10

Version formatı:

bits 7..4 = Major Version
bits 3..0 = Minor Version

Tanım:

#define DEOS_PROTOCOL_VERSION 0x10


============================================================
6. 29-BIT EXTENDED CAN IDENTIFIER
============================================================

CAN Identifier tam olarak şu yapıda:

Bits 28..26 : Priority
Bits 25..22 : Message Class
Bits 21..16 : Service ID
Bits 15..8  : Destination Node
Bits 7..0   : Source Node

Bit genişlikleri:

Priority:
3 bit

Message Class:
4 bit

Service:
6 bit

Destination:
8 bit

Source:
8 bit

Toplam:

29 bit


CAN ID formülü:

CAN_ID =
    ((uint32_t)priority      << 26) |
    ((uint32_t)message_class << 22) |
    ((uint32_t)service       << 16) |
    ((uint32_t)destination   << 8)  |
    ((uint32_t)source);


Shift/mask tanımlarını deos_icd.h içerisinde açık şekilde oluştur.

Örneğin mantıksal olarak:

DEOS_CAN_PRIO_SHIFT
DEOS_CAN_CLASS_SHIFT
DEOS_CAN_SERVICE_SHIFT
DEOS_CAN_DEST_SHIFT
DEOS_CAN_SOURCE_SHIFT

ve gerekli mask'ler.


============================================================
7. PRIORITY REGISTRY
============================================================

typedef enum
{
    DEOS_PRIO_EMERGENCY        = 0,
    DEOS_PRIO_CRITICAL_CONTROL = 1,
    DEOS_PRIO_CONTROL          = 2,
    DEOS_PRIO_STATUS           = 3,
    DEOS_PRIO_CONFIG           = 4,
    DEOS_PRIO_NETWORK          = 5,
    DEOS_PRIO_LOW              = 6,
    DEOS_PRIO_BACKGROUND       = 7

} deos_priority_t;


Düşük numeric identifier CAN arbitration nedeniyle daha yüksek bus priority
anlamına gelir.


============================================================
8. NODE REGISTRY
============================================================

typedef enum
{
    DEOS_NODE_INVALID        = 0x00,

    DEOS_NODE_TEXTUAL        = 0x01,
    DEOS_NODE_GROUND_CONTROL = 0x02,
    DEOS_NODE_MAIN_STM32     = 0x03,
    DEOS_NODE_MICRO_ROS      = 0x04,

    DEOS_NODE_TRACTION       = 0x10,
    DEOS_NODE_STEERING       = 0x11,
    DEOS_NODE_BRAKE          = 0x12,

    DEOS_NODE_BMS_MAIN       = 0x20,
    DEOS_NODE_BMS_AUX        = 0x21,

    DEOS_NODE_BMS_GATEWAY    = 0x30,

    DEOS_NODE_DIAG_TOOL      = 0xF0,

    DEOS_NODE_BROADCAST      = 0xFF

} deos_node_id_t;


Local node olarak:

DEOS_NODE_INVALID

kabul edilmemeli.

DEOS_NODE_BROADCAST

local node olarak kabul edilmemeli.


============================================================
9. MESSAGE CLASS REGISTRY
============================================================

typedef enum
{
    DEOS_CLASS_SAFETY   = 0x0,
    DEOS_CLASS_COMMAND  = 0x1,
    DEOS_CLASS_STATUS   = 0x2,
    DEOS_CLASS_EVENT    = 0x3,
    DEOS_CLASS_REQUEST  = 0x4,
    DEOS_CLASS_RESPONSE = 0x5,
    DEOS_CLASS_CONFIG   = 0x6,
    DEOS_CLASS_NETWORK  = 0x7

} deos_message_class_t;


0x8 ... 0xF:

Reserved.

Receiver bunları normal supported application class olarak değerlendirmemeli.


============================================================
10. SERVICE REGISTRY
============================================================

typedef enum
{
    DEOS_SERVICE_SYSTEM      = 0x00,
    DEOS_SERVICE_TRACTION    = 0x01,
    DEOS_SERVICE_STEERING    = 0x02,
    DEOS_SERVICE_BRAKE       = 0x03,
    DEOS_SERVICE_BMS         = 0x04,
    DEOS_SERVICE_CALIBRATION = 0x05,
    DEOS_SERVICE_DIAGNOSTIC  = 0x06

} deos_service_id_t;


0x07 ... 0x3F:

Reserved.


============================================================
11. SYSTEM COMMANDS
============================================================

typedef enum
{
    DEOS_CMD_SYSTEM_GET_STATE     = 0x01,
    DEOS_CMD_SYSTEM_SET_STATE     = 0x02,
    DEOS_CMD_SYSTEM_GET_MODE      = 0x03,
    DEOS_CMD_SYSTEM_SET_MODE      = 0x04,
    DEOS_CMD_SYSTEM_RESET_NODE    = 0x05,
    DEOS_CMD_SYSTEM_GET_NODE_INFO = 0x06,
    DEOS_CMD_SYSTEM_HEARTBEAT     = 0x07,
    DEOS_CMD_SYSTEM_PING          = 0x08,
    DEOS_CMD_SYSTEM_PONG          = 0x09

} deos_system_command_t;


============================================================
12. TRACTION COMMANDS
============================================================

typedef enum
{
    DEOS_CMD_TRACTION_SET_TARGET_SPEED = 0x01,
    DEOS_CMD_TRACTION_GET_STATUS       = 0x02

} deos_traction_command_t;


============================================================
13. STEERING COMMANDS
============================================================

typedef enum
{
    DEOS_CMD_STEERING_SET_TARGET_ANGLE = 0x01,
    DEOS_CMD_STEERING_GET_STATUS       = 0x02

} deos_steering_command_t;


============================================================
14. BRAKE COMMANDS
============================================================

typedef enum
{
    DEOS_CMD_BRAKE_SET_TARGET = 0x01,
    DEOS_CMD_BRAKE_GET_STATUS = 0x02

} deos_brake_command_t;


============================================================
15. BMS COMMANDS
============================================================

typedef enum
{
    DEOS_CMD_BMS_GET_STATUS = 0x01,
    DEOS_CMD_BMS_STATUS     = 0x02

} deos_bms_command_t;


============================================================
16. CALIBRATION COMMANDS
============================================================

typedef enum
{
    DEOS_CMD_CAL_START        = 0x01,
    DEOS_CMD_CAL_ABORT        = 0x02,
    DEOS_CMD_CAL_GET_STATUS   = 0x03,
    DEOS_CMD_CAL_SAVE_RESULT  = 0x04,
    DEOS_CMD_CAL_CLEAR_RESULT = 0x05

} deos_calibration_command_t;


============================================================
17. DIAGNOSTIC COMMANDS
============================================================

typedef enum
{
    DEOS_CMD_DIAG_GET_HEALTH   = 0x01,
    DEOS_CMD_DIAG_GET_FAULTS   = 0x02,
    DEOS_CMD_DIAG_CLEAR_FAULTS = 0x03

} deos_diagnostic_command_t;


============================================================
18. PARAMETER COMMANDS
============================================================

typedef enum
{
    DEOS_CMD_GET_PARAMETER      = 0xE0,
    DEOS_CMD_SET_PARAMETER      = 0xE1,
    DEOS_CMD_SAVE_PARAMETERS    = 0xE2,
    DEOS_CMD_RESTORE_DEFAULTS   = 0xE3,
    DEOS_CMD_GET_PARAMETER_INFO = 0xE4

} deos_parameter_command_t;


Parameter ID:

16 bit.

Parameter ID global değildir.

Semantic uniqueness:

Service ID + Parameter ID


============================================================
19. STATE REGISTRY
============================================================

typedef enum
{
    DEOS_STATE_INIT        = 0x00,
    DEOS_STATE_STANDBY     = 0x01,
    DEOS_STATE_READY       = 0x02,
    DEOS_STATE_ACTIVE      = 0x03,
    DEOS_STATE_CALIBRATING = 0x04,
    DEOS_STATE_SAFE        = 0x05,
    DEOS_STATE_FAULT       = 0x06,

    DEOS_STATE_UNKNOWN     = 0xFF

} deos_state_t;


============================================================
20. MODE REGISTRY
============================================================

typedef enum
{
    DEOS_MODE_NORMAL     = 0x00,
    DEOS_MODE_MANUAL     = 0x01,
    DEOS_MODE_AUTONOMOUS = 0x02,
    DEOS_MODE_SERVICE    = 0x03,
    DEOS_MODE_TEST       = 0x04,

    DEOS_MODE_UNKNOWN    = 0xFF

} deos_mode_t;


============================================================
21. RESPONSE RESULT CODE REGISTRY
============================================================

typedef enum
{
    DEOS_RESULT_SUCCESS           = 0x00,
    DEOS_RESULT_UNSUPPORTED       = 0x01,
    DEOS_RESULT_INVALID_PARAMETER = 0x02,
    DEOS_RESULT_OUT_OF_RANGE      = 0x03,
    DEOS_RESULT_NOT_ALLOWED       = 0x04,
    DEOS_RESULT_BUSY              = 0x05,
    DEOS_RESULT_NOT_READY         = 0x06,
    DEOS_RESULT_INTERNAL_ERROR    = 0x07

} deos_result_t;


NOT:

Response exact payload layout henüz frozen değildir.

Bu enum tanımlansın fakat kafadan generic RESPONSE payload formatı oluşturma.


============================================================
22. DEOS PAYLOAD FORMAT
============================================================

CAN-FD application data:

Byte 0:
Protocol Version

Byte 1:
Sequence

Byte 2:
Command ID

Byte 3..N:
Command-specific Payload


Yani:

[VERSION][SEQUENCE][COMMAND][PAYLOAD...]


Application layer içerisinde ayrıca:

- AA55 header
- Source
- Destination
- Service
- Priority
- Length
- CRC

TAŞINMAYACAK.


Source / Destination / Service / Priority:

CAN Extended Identifier içerisinde.


Frame length:

CAN-FD DLC üzerinden.


CAN-FD data maximum:

64 bytes


Command-specific payload maximum:

61 bytes


============================================================
23. DEOS MESSAGE STRUCT
============================================================

deos_types.h içerisinde aşağıdaki mantığa uygun struct oluştur:

typedef struct
{
    deos_priority_t priority;
    deos_message_class_t message_class;
    deos_service_id_t service;

    deos_node_id_t destination;
    deos_node_id_t source;

    uint8_t version;
    uint8_t sequence;
    uint8_t command;

    uint8_t payload[DEOS_MAX_PAYLOAD_LEN];
    uint8_t payload_len;

} deos_message_t;


ÖNEMLİ:

message.payload yalnız command-specific payload olacak.

Örneğin PING'de:

payload[0..3] = Ping ID

Version / Sequence / Command payload içine tekrar konmayacak.


============================================================
24. BYTE ORDER
============================================================

Wire format default:

Little-Endian


Explicit helper fonksiyonlar oluştur.

Örneğin:

void deos_put_u16_le(uint8_t *dst, uint16_t value);
void deos_put_i16_le(uint8_t *dst, int16_t value);
void deos_put_u32_le(uint8_t *dst, uint32_t value);

uint16_t deos_get_u16_le(const uint8_t *src);
int16_t  deos_get_i16_le(const uint8_t *src);
uint32_t deos_get_u32_le(const uint8_t *src);


Zephyr sys_put_le16 / sys_get_le16 gibi uygun helper sağlıyorsa
mevcut API'yi kullanabilirsin.

Ancak byte order davranışı açık olmalı.


============================================================
25. SEQUENCE
============================================================

Her node tek bir 8-bit TX sequence counter kullanır.

Sequence her yeni outbound DEOS message'da artırılır.

Overflow:

255 -> 0

normaldir.

Receiver:

sequence gap
duplicate sequence
sequence wrap

gibi durumları v1.0'da protocol error olarak değerlendirmeyecek.

Sequence gelecekte tracing/freshness için kullanılabilir.

PING/PONG correlation sequence ile YAPILMAYACAK.


============================================================
26. CONFIG STRUCT
============================================================

Public configuration için sade bir struct oluştur.

Örneğin:

struct deos_config
{
    deos_node_id_t node_id;
    const struct device *can_dev;
    bool router_enabled;
};


Gerekirse heartbeat için:

bool heartbeat_enabled;
uint32_t heartbeat_period_ms;

eklenebilir.

Fakat config struct'ı gereksiz büyütme.


============================================================
27. PUBLIC API
============================================================

deos.h application'ın ana public header'ı olsun.

Application mümkün olduğunca yalnız:

#include <deos/deos.h>

kullansın.


Önerilen API:

int deos_init(const struct deos_config *config);

int deos_start(void);

int deos_send(
    deos_node_id_t destination,
    deos_priority_t priority,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    const void *payload,
    size_t payload_len);

int deos_send_ping(
    deos_node_id_t destination,
    uint32_t ping_id);

int deos_register_handler(
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    deos_message_handler_t handler,
    void *user_data);


Eğer init/start ayrımını anlamsız bulursan tek init fonksiyonuna indirebilirsin.

Ama public API sade kalsın.


============================================================
28. APPLICATION HANDLER TYPE
============================================================

typedef int (*deos_message_handler_t)(
    const deos_message_t *message,
    void *user_data);


Application örneği:

deos_register_handler(
    DEOS_CLASS_COMMAND,
    DEOS_SERVICE_STEERING,
    DEOS_CMD_STEERING_SET_TARGET_ANGLE,
    steering_target_handler,
    NULL);


============================================================
29. HANDLER REGISTRY
============================================================

Dynamic allocation kullanma.

Static handler table kullan.

Örneğin:

#define DEOS_MAX_HANDLERS 32


Her entry:

message_class
service
command
handler
user_data

tutsun.


Duplicate registration:

-EEXIST veya uygun errno


Table full:

-ENOSPC


Handler bulunamadı:

-ENOTSUP


Application handler table başlangıçta init sırasında register edileceği
varsayılabilir.

Gereksiz runtime locking ekleme.


============================================================
30. DEOS INIT
============================================================

deos_init():

- config NULL kontrolü
- CAN device NULL kontrolü
- CAN device ready kontrolü
- local node ID doğrulaması
- INVALID reject
- BROADCAST reject
- config internal olarak kaydedilsin
- sequence sıfırlansın
- handler registry sıfırlansın
- RX infrastructure hazırlansın
- gerekli log basılsın


CAN controller support uygunsa CAN-FD mode başlatılsın.

Board CAN-FD support etmiyorsa hata dönmesi kabul edilebilir.

Classic CAN'e fallback YAPMA.


============================================================
31. CAN DEVICE
============================================================

CAN device Devicetree üzerinden alınmalı.

Hardcoded peripheral base address yazma.

Application isterse config.can_dev ile device pointer verebilsin.

Sample application uygun DT node üzerinden cihazı alsın.

device_is_ready() kontrolü yap.


============================================================
32. CAN-FD BITRATE
============================================================

Proje hedefi nominal CAN arbitration bitrate:

250 kbit/s


CAN-FD data phase için ayrı bitrate kullanılabilir.

Data bitrate merkezi bir configuration üzerinden belirlenmeli.

Exact değer project config/devicetree'den alınabilecek şekilde yaz.

Farklı dosyalarda magic number kullanma.


============================================================
33. CAN RX CALLBACK
============================================================

CAN RX callback mümkün olduğunca kısa olacak.

Callback içerisinde:

- frame decode ETME
- dispatcher çağırma
- application callback çağırma
- PING/PONG işleme
- uzun LOG basma

YAPMA.


Sadece:

incoming CAN-FD frame
        |
        v
static RX queue


Static Zephyr message queue kullan:

K_MSGQ_DEFINE

veya eşdeğer static allocation.


Queue full ise:

- callback block olmasın
- frame drop edilebilir
- rx_drop_count artırılabilir
- warning rate limited olabilir


============================================================
34. DEOS RX THREAD
============================================================

DEOS Core kendi RX worker thread'ini kullansın.

Akış:

while (1)
{
    RX queue'dan CAN-FD frame bekle

    frame decode et

    invalid ise drop

    valid ise dispatcher'a gönder
}


Static thread kullan.

K_THREAD_DEFINE veya k_thread_create tercih edebilirsin.

Heap kullanma.


Thread stack:

DEOS_RX_THREAD_STACK_SIZE

Thread priority:

DEOS_RX_THREAD_PRIORITY

gibi merkezi constant/config üzerinden tanımlansın.


============================================================
35. CAN-FD FRAME DECODE
============================================================

Saf/test edilebilir decode kodu oluştur.

Akış:

struct can_frame
        |
        v
Extended ID kontrolü
        |
        v
CAN-FD frame kontrolü
        |
        v
29-bit ID decode
        |
        v
payload length kontrolü
        |
        v
Version
Sequence
Command
Payload
        |
        v
deos_message_t


Minimum frame data length:

3 byte


Validation:

- Extended frame olmalı
- CAN-FD frame olmalı
- data length >= 3
- data length <= 64
- priority valid
- message class valid
- service valid
- destination != INVALID
- source != INVALID
- payload <= 61
- protocol major version compatible


Unknown command:

decode aşamasında invalid sayılmamalı.

Command dispatch seviyesinde değerlendirilir.


============================================================
36. CAN ID CODEC
============================================================

Hardware bağımsız saf fonksiyonlar oluştur.

Örneğin:

int deos_can_id_encode(
    deos_priority_t priority,
    deos_message_class_t message_class,
    deos_service_id_t service,
    deos_node_id_t destination,
    deos_node_id_t source,
    uint32_t *can_id);


int deos_can_id_decode(
    uint32_t can_id,
    deos_message_t *message);


Decode yalnız CAN ID metadata alanlarını doldurabilir.

İsimleri daha temiz tasarlayabiliyorsan değiştir.


============================================================
37. FRAME ENCODE
============================================================

deos_message_t
        |
        v
validation
        |
        v
CAN ID encode
        |
        v
Byte0 Version
Byte1 Sequence
Byte2 Command
Byte3.. Payload
        |
        v
CAN-FD struct can_frame


Frame:

Extended ID

CAN-FD flag

uygun şekilde ayarlansın.


Payload length:

3 + command_payload_len


64 byte sınırı kesin kontrol edilsin.


============================================================
38. deos_send()
============================================================

Application source belirlemeyecek.

Normal send:

source = local node

version = DEOS_PROTOCOL_VERSION

sequence = next sequence


deos_send():

destination
priority
message_class
service
command
payload
payload_len

almalı.


Application'ın source spoof etmesine normal API'den izin verme.


============================================================
39. TX CONCURRENCY
============================================================

Birden fazla application thread aynı anda deos_send() çağırabilir.

TX sequence thread-safe olmalı.

Zephyr atomic/spinlock/mutex arasından hafif ve doğru çözümü kullan.

Sequence increment race condition üretmemeli.


CAN send serialization gerekiyorsa bunu güvenli şekilde yap.

Gereksiz karmaşık TX scheduler yazma.


============================================================
40. DISPATCHER
============================================================

Dispatcher'ın ilk işi Destination değerlendirmesi.


Normal node:

destination == local node
    -> local handling

destination == BROADCAST
    -> broadcast-safe local handling

başka destination
    -> ignore/drop


Main STM32:

local node == DEOS_NODE_MAIN_STM32

ve destination != MAIN_STM32 ise:

router'a gönderilebilir.


Dispatcher sonra:

Message Class
    |
    v
Common DEOS handler?
    |
    +-- NETWORK / SYSTEM
    |
    +-- common protocol
    |
    +-- registered application handler


============================================================
41. COMMON DEOS VS APPLICATION
============================================================

PING gibi DEOS protokol davranışları application'a gönderilmeyecek.

Örneğin Steering application şunlarla uğraşmamalı:

- Ping ID parsing
- PONG response
- Sequence
- CAN ID
- Source/Destination encode
- Version


Application sadece:

steering_set_target_angle()

gibi kendi işiyle ilgilenmeli.


============================================================
42. PING
============================================================

PING:

Priority:
DEOS_PRIO_NETWORK

Message Class:
DEOS_CLASS_NETWORK

Service:
DEOS_SERVICE_SYSTEM

Command:
DEOS_CMD_SYSTEM_PING


Command-specific payload:

uint32_t Ping ID


Little-Endian.


Frame application bytes:

Byte0 = Protocol Version
Byte1 = Sequence
Byte2 = PING Command
Byte3 = PingID bits 7..0
Byte4 = PingID bits 15..8
Byte5 = PingID bits 23..16
Byte6 = PingID bits 31..24


Total:

7 byte


Public API:

deos_send_ping(destination, ping_id);


Broadcast destination:

reject.

-EINVAL dön.


============================================================
43. PONG
============================================================

Geçerli ve local node'a yöneltilmiş PING geldiğinde DEOS Core otomatik PONG
üretmeli.

Application handler çağırmaya gerek yok.


PONG:

Priority:
DEOS_PRIO_NETWORK

Message Class:
DEOS_CLASS_NETWORK

Service:
DEOS_SERVICE_SYSTEM

Command:
DEOS_CMD_SYSTEM_PONG


Source:

local node

Destination:

PING Source


Ping ID:

PING'den aynen alınacak.


ÖNEMLİ:

PONG sequence değeri PING sequence'den kopyalanmayacak.

PONG gönderen node kendi normal TX sequence counter'ını kullanacak.


============================================================
44. PONG RECEIVE
============================================================

PONG geldiğinde core:

- source node
- Ping ID

bilgilerini decode etsin.


İlk sürümde LOG_DBG yeterli olabilir.

İstersen optional callback oluştur:

deos_pong_callback_t

Ancak büyük transaction manager oluşturma.


============================================================
45. RTT
============================================================

RTT sender üzerinde local timestamps ile ölçülebilir:

RTT = Tpong - Tping


Ancak ilk sürümde full outstanding transaction manager şart değil.

Architecture ileride bunun eklenmesine izin versin.

Ping correlation:

yalnız Ping ID.


============================================================
46. BROADCAST PING
============================================================

Broadcast PING kesinlikle geçersiz.

Destination:

DEOS_NODE_BROADCAST

ile PING gönderilemez.


Amaç response burst oluşmasını engellemektir.


============================================================
47. HEARTBEAT
============================================================

HEARTBEAT:

Priority:
DEOS_PRIO_NETWORK

Class:
DEOS_CLASS_NETWORK

Service:
DEOS_SERVICE_SYSTEM

Command:
DEOS_CMD_SYSTEM_HEARTBEAT


Ancak exact HEARTBEAT payload formatı ICD'de tam dondurulmamış.

Bu nedenle kafadan normatif payload oluşturma.

Heartbeat altyapısını hazırlayabilirsin.

Payload length 0 ile provisional implementation kullanırsan comment'te açıkça:

"provisional, payload not frozen by ICD"

yaz.


============================================================
48. GET_STATE / SET_STATE
============================================================

SYSTEM command ID'leri mevcut.

Ancak exact payload / response details tamamen freeze değil.

Bu nedenle:

- registry oluştur
- handler register edilebilsin
- application bu command'ları işleyebilsin

Core generic state machine UYDURMASIN.


============================================================
49. REQUEST / RESPONSE
============================================================

REQUEST ve RESPONSE ayrı message class'tır.

Response result enum'u tanımlı olsun.

Fakat RESPONSE payload layout henüz frozen değildir.

Generic RESPONSE packet formatı kafadan oluşturma.


============================================================
50. CONFIG / PARAMETERS
============================================================

CONFIG ayrı bir Service DEĞİLDİR.

Örneğin:

Message Class = CONFIG
Service = STEERING
Command = SET_PARAMETER


Parameter commands:

0xE0...0xE4

registry içinde bulunacak.


Exact parameter payload formatı tam frozen değilse kendi standardını normatif
olarak ekleme.


============================================================
51. ROUTER
============================================================

Main STM32 özel davranır.

Ana routing key:

Destination Node


Main başka node'a ait mesaj geldiğinde:

message_class
service
command

semantiğini yorumlamak zorunda değildir.


Örneğin:

Steering -> Textual

Main:

Destination = TEXTUAL

görür

ve uygun transport'a route eder.


Router abstraction oluştur.


============================================================
52. TRANSPORT TYPE
============================================================

İleride route abstraction için:

typedef enum
{
    DEOS_TRANSPORT_LOCAL,
    DEOS_TRANSPORT_CAN_FD,
    DEOS_TRANSPORT_ETHERNET,
    DEOS_TRANSPORT_LORA

} deos_transport_t;


Gerçek implementasyon şu an:

CAN-FD


Ethernet/LoRa:

stub/interface/TODO olabilir.


============================================================
53. ROUTE TABLE
============================================================

Basit static route table tasarlanabilir.

Örneğin:

struct deos_route_entry
{
    deos_node_id_t destination;
    deos_transport_t transport;
};


Dynamic memory kullanma.


============================================================
54. ROUTING LOOP KURALI
============================================================

Shared CAN-FD segmentinde bir frame zaten destination tarafından doğrudan
alınabiliyorsa Main aynı frame'i aynı CAN-FD transport'a tekrar basmamalı.

Duplicate frame ve loop üretme.


Router incoming transport bilgisini bilmeli.

Örneğin:

deos_route_message(message, incoming_transport);


============================================================
55. MAIN LOCAL MESSAGE
============================================================

Destination:

DEOS_NODE_MAIN_STM32

ise:

router'a gitmeyecek.

Local dispatcher tarafından işlenecek.


============================================================
56. BMS GATEWAY SOURCE RULE
============================================================

Gateway kendi mesajını gönderirken:

Source = DEOS_NODE_BMS_GATEWAY


Gateway BMS #1 adına çevrilmiş DEOS message üretirken:

Source = DEOS_NODE_BMS_MAIN


Gateway BMS #2 adına üretirken:

Source = DEOS_NODE_BMS_AUX


Normal public deos_send():

source spoof'a izin vermeyecek.


Gateway/router için gerekirse private/internal:

deos_send_as()

oluştur.

PUBLIC API'ye koyma.


============================================================
57. PHYSICAL VALUE FORMAT
============================================================

ICD başlangıç wire formatları:

Steering Target Angle:
int16_t
scale = 0.01 degree

Vehicle Target Speed:
uint16_t
scale = 0.01 m/s

Brake Target:
uint16_t
scale = 0.01 %

BMS Voltage:
uint16_t
scale = 0.01 V

BMS Current:
int16_t
scale = 0.1 A

BMS SOC:
uint16_t
scale = 0.01 %

Temperature:
int16_t
scale = 0.1 °C


DEOS Core control logic yazmayacak.

Yalnız wire conversion helper gerekiyorsa scaled integer yaklaşımını koru.


============================================================
58. EXAMPLE STEERING HANDLER
============================================================

Sample application içerisinde gerçek motor sürmeden örnek handler oluştur.

Örneğin:

static int steering_target_handler(
    const deos_message_t *message,
    void *user_data)
{
    if (message->payload_len != 2) {
        return -EMSGSIZE;
    }

    int16_t raw = deos_get_i16_le(message->payload);

    LOG_INF("Steering target raw = %d", raw);

    return 0;
}


Application'ın CAN frame detaylarını bilmesine gerek olmasın.


============================================================
59. APPLICATION BOUNDARY
============================================================

İstenen kullanım tarzı:

Communication person:

DEOS Core


Control person:

steering_control.c
brake_control.c
traction_control.c


Örneğin control tarafı:

void steering_set_target_angle(float angle_deg);


DEOS handler:

wire payload decode eder
        |
        v
steering_set_target_angle()


DEOS callback içerisinde uzun PID loop çalıştırma.

Application setpoint update edip kendi control thread'inde çalışsın.


============================================================
60. THREAD MODEL
============================================================

Örneğin Steering node:

DEOS RX Thread
    - communication
    - decode
    - dispatch
    - ping/pong

Steering Control Thread
    - encoder
    - PID
    - motor control


Bu thread'ler bağımsız olmalı.


============================================================
61. ERROR HANDLING
============================================================

Linux/Zephyr errno-style negative errors kullan.

Örnek:

0
-EINVAL
-ENODEV
-ENOTSUP
-ENOSPC
-EEXIST
-EMSGSIZE
-EPROTO
-EIO
-EAGAIN


Tutarlı kullan.


============================================================
62. LOGGING
============================================================

Zephyr LOG kullan.

Örneğin:

LOG_MODULE_REGISTER(deos_core)
LOG_MODULE_REGISTER(deos_rx)
LOG_MODULE_REGISTER(deos_tx)
LOG_MODULE_REGISTER(deos_dispatch)
LOG_MODULE_REGISTER(deos_network)
LOG_MODULE_REGISTER(deos_router)


Her frame'i INFO seviyesinde spamleme.

Normal packet trace:

LOG_DBG


Initialization:

LOG_INF


Malformed/drop:

LOG_WRN


Critical error:

LOG_ERR


============================================================
63. MEMORY MODEL
============================================================

malloc YOK.

calloc YOK.

heap YOK.

Static allocation kullan.

Static:

- RX queue
- handler registry
- thread stack
- route table
- protocol state


============================================================
64. PUBLIC / PRIVATE AYRIMI
============================================================

Public:

lib/deos_core/include/deos/


Private:

lib/deos_core/src/deos_internal.h


Application private protocol state'e erişmemeli.


============================================================
65. DOSYA YAPISI
============================================================

Gerekirse mevcut klasör yapısını şu hale genişlet:

lib/deos_core/
│
├── CMakeLists.txt
│
├── include/
│   └── deos/
│       ├── deos.h
│       ├── deos_icd.h
│       └── deos_types.h
│
└── src/
    ├── deos_internal.h
    ├── deos_core.c
    ├── deos_codec.c
    ├── deos_rx.c
    ├── deos_tx.c
    ├── deos_dispatch.c
    ├── deos_network.c
    └── deos_router.c


============================================================
66. deos_icd.h
============================================================

Sadece normatif DEOS protocol definition'ları.

İçeriği:

- protocol version
- max frame/payload constants
- CAN ID masks/shifts
- node enum
- priority enum
- class enum
- service enum
- command enums
- parameter command enum
- result enum
- state enum
- mode enum


Application logic burada OLMAMALI.


============================================================
67. deos_types.h
============================================================

Runtime/public types.

Örneğin:

- deos_message_t
- deos_config
- deos_message_handler_t
- transport enum gerekiyorsa
- pong callback type gerekiyorsa


============================================================
68. deos.h
============================================================

Public API.

Application mümkün olduğunca yalnız bunu include edecek.

Internal functions burada OLMAMALI.


============================================================
69. deos_internal.h
============================================================

Private declarations.

Örneğin:

deos_encode_frame()
deos_decode_frame()
deos_dispatch()
deos_handle_network()
deos_next_sequence()
deos_route_message()

gibi.


============================================================
70. deos_core.c
============================================================

Sorumluluk:

- init
- runtime configuration
- local node
- sequence
- lifecycle
- global internal protocol state


============================================================
71. deos_codec.c
============================================================

Sorumluluk:

- CAN ID encode/decode
- frame encode/decode
- wire endian helpers
- validation


Hardware bağımlılığı minimum olsun.


============================================================
72. deos_rx.c
============================================================

Sorumluluk:

- CAN-FD RX filter
- CAN callback
- RX queue
- RX worker thread


============================================================
73. deos_tx.c
============================================================

Sorumluluk:

- deos_send()
- CAN-FD send
- frame encode
- sequence
- TX concurrency


============================================================
74. deos_dispatch.c
============================================================

Sorumluluk:

- destination
- broadcast
- local/common/application dispatch
- handler registry


============================================================
75. deos_network.c
============================================================

Sorumluluk:

- PING
- PONG
- HEARTBEAT
- NETWORK/SYSTEM common behaviour


============================================================
76. deos_router.c
============================================================

Sorumluluk:

- Main STM32 route decision
- incoming transport
- route table
- transport abstraction


============================================================
77. CAN FILTER
============================================================

Normal endpoint node için mümkünse hardware filtering kullanılabilir.

Ancak correctness önce gelir.

Normal node:

local destination
broadcast

frame'lerini alabilir.


Main STM32 router:

CAN-FD segmentindeki route edilmesi gereken frame'leri görmeli.

Sadece kendi destination ID'sine filter koyup diğer mesajları kaybetmemeli.


============================================================
78. PING/PONG AUTOMATIC HANDLING
============================================================

PING/PONG application handler registry'den bağımsız common behaviour olsun.

Dispatcher:

NETWORK
+
SYSTEM
+
PING

görünce:

deos_handle_ping()


ve automatic PONG.


PONG:

deos_handle_pong()


Application bu mekanizmayı bilmek zorunda değil.


============================================================
79. BROADCAST RULE
============================================================

Broadcast node:

0xFF


Normal broadcast mesajlar ilgili semantic uygunsa local olarak işlenebilir.

Ancak broadcast response storm oluşturabilecek common behaviours dikkatli
olmalı.


PING broadcast kesin reject.


============================================================
80. SAFETY
============================================================

DEOS_CLASS_SAFETY tanımlı olsun.

Ancak safety command payloadları ICD'de freeze değil.

Kendi safety message formatını uydurma.

Application handler üzerinden taşınabilsin.


============================================================
81. HEARTBEAT THREAD
============================================================

Eğer heartbeat periodic gönderilecekse ayrı lightweight delayed work veya
thread tercih edebilirsin.

Ancak sırf heartbeat için gereksiz dedicated thread oluşturmak yerine
Zephyr delayed work daha mantıklıysa onu kullan.

Exact payload freeze olmadığı için basit kal.


============================================================
82. K_WORK / THREAD SEÇİMİ
============================================================

Zephyr primitive'lerini uygun kullan.

RX:

thread + queue mantıklı.


Heartbeat:

delayed work olabilir.


Application control:

DEOS Core sorumluluğu değil.


============================================================
83. TEST EDİLEBİLİR FONKSİYONLAR
============================================================

Hardware gerektirmeyen saf fonksiyonları açık ayır:

- CAN ID encode
- CAN ID decode
- endian
- message validation
- frame payload serialization
- frame payload deserialization

İleride ztest ile test edilebilsin.


============================================================
84. TESTLER
============================================================

Mümkünse tests/protocol oluştur.

Test case'ler:

1.
Priority = CONTROL
Class = COMMAND
Service = STEERING
Destination = STEERING
Source = MAIN

CAN ID encode/decode round trip.


2.
Maximum valid 29-bit ID fields.


3.
Invalid priority reject.


4.
Invalid message class reject.


5.
Invalid service reject.


6.
INVALID source reject.


7.
INVALID destination reject.


8.
local node INVALID init reject.


9.
local node BROADCAST init reject.


10.
PING encode:

4-byte Ping ID


11.
Broadcast PING reject.


12.
Sequence:

254
255
0


13.
Message payload:

61 byte valid


14.
62 byte command payload reject.


15.
Frame total:

64 byte valid.


============================================================
85. SAMPLE APPLICATION
============================================================

src/main.c örnek kullanım göstermeli.

Yaklaşık:

static int steering_target_handler(
    const deos_message_t *message,
    void *user_data)
{
    ...
}


int main(void)
{
    const struct device *can_dev = ...;

    struct deos_config config = {
        .node_id = DEOS_NODE_STEERING,
        .can_dev = can_dev,
        .router_enabled = false
    };

    int ret = deos_init(&config);

    if (ret != 0) {
        LOG_ERR(...);
        return ret;
    }

    deos_register_handler(
        DEOS_CLASS_COMMAND,
        DEOS_SERVICE_STEERING,
        DEOS_CMD_STEERING_SET_TARGET_ANGLE,
        steering_target_handler,
        NULL);

    ret = deos_start();

    if (ret != 0) {
        LOG_ERR(...);
        return ret;
    }

    while (1) {
        k_sleep(K_FOREVER);
    }
}


============================================================
86. SAMPLE PING
============================================================

Örnek olarak compile-time kapalı bir test olabilir:

#define DEOS_SAMPLE_PING_ENABLED 0


Açılırsa:

deos_send_ping(
    DEOS_NODE_MAIN_STM32,
    0x12345678
);


Ancak default olarak CAN bus yokken sürekli error spam üretme.


============================================================
87. README.md
============================================================

Root README oluştur.

İçerik:

- DEOS Core nedir
- architecture
- CAN-FD requirement
- 29-bit ID layout
- directory structure
- initialization
- handler registration
- send example
- ping example
- routing concept
- application/core separation
- current development status


Mevcut F439 board'un CAN-FD desteklememesi durumunu development-board mismatch
olarak açıkça yazabilirsin.

Protocol limitation gibi sunma.


============================================================
88. docs/architecture.md
============================================================

Mimariyi ASCII diagram ile anlat.


RX:

CAN-FD Bus
    |
    v
Zephyr CAN RX Callback
    |
    v
Static RX Queue
    |
    v
DEOS RX Thread
    |
    v
DEOS Decoder
    |
    v
Dispatcher
    |
    +----------> Router
    |
    +----------> Common DEOS
    |               |
    |               +--> PING/PONG
    |
    +----------> Application Handler


TX:

Application/Common DEOS
        |
        v
deos_send()
        |
        v
DEOS Encoder
        |
        v
CAN-FD Frame
        |
        v
Zephyr CAN
        |
        v
CAN-FD Bus


============================================================
89. ICD'DE HENÜZ FREEZE OLMAYAN ALANLAR
============================================================

Aşağıdaki konularda kendi standardını normatif DEOS davranışı gibi UYDURMA:

- her mesajın tam payload byte map'i
- command physical min/max değerleri
- command periodları
- command timeoutları
- hangi command RESPONSE zorunlu
- RESPONSE exact payload structure
- final safety payloads
- calibration exact payloads
- final BMS status signal list
- parameter value type wire placement
- firmware update / bootloader protocol
- future sequence semantics
- Ethernet IP/port mapping
- LoRa framing
- PING timeout/retry policy


Bunlar için:

TODO

veya:

PROVISIONAL

etiketi kullan.


============================================================
90. BMS HAKKINDA
============================================================

Bu ICD sürümünde BMS command registry:

DEOS_CMD_BMS_GET_STATUS
DEOS_CMD_BMS_STATUS


Bunun dışındaki cell-list, temp-list gibi command'ları şu aşamada ekleme.

ICD dışında yeni protocol command uydurma.


============================================================
91. KOD STİLİ
============================================================

Kod:

- sade
- deterministic
- embedded friendly
- Zephyr idiomatic
- static allocation
- minimum coupling
- minimum global state
- readable

olsun.


Naming:

DEOS_NODE_...
DEOS_PRIO_...
DEOS_CLASS_...
DEOS_SERVICE_...
DEOS_CMD_...
DEOS_STATE_...
DEOS_MODE_...


Types:

deos_node_id_t
deos_priority_t
deos_message_class_t
deos_service_id_t
deos_state_t
deos_mode_t
deos_result_t
deos_message_t
deos_message_handler_t


============================================================
92. YAPILMAYACAKLAR
============================================================

Classic CAN desteği YOK.

Classic CAN fallback YOK.

Fragmentation YOK.

malloc YOK.

heap YOK.

Direct STM32 register programming YOK.

Fake FDCAN implementation YOK.

Control/PID logic YOK.

Motor driver logic YOK.

Encoder logic YOK.

Application callback içinde blocking loop YOK.

RX CAN callback içinde heavy processing YOK.

Source spoof normal public API'de YOK.

Broadcast PING YOK.

Silent truncation YOK.

ICD'de olmayan payloadları normatifmiş gibi uydurmak YOK.


============================================================
93. BOARD / BUILD KONUSU
============================================================

Mevcut board:

nucleo_f439zi


Bu board CAN-FD/FDCAN desteklemiyorsa CAN-FD transport kodu compile etmeyebilir.

Bu kabul edilebilir.

Bunun için DEOS Core'u Classic CAN'e dönüştürme.

Gerekirse:

- hardware-independent protocol files compile/test et
- CAN-FD specific build limitation'ını raporla
- CAN-FD capable target board önerisi için yalnız TODO bırak


Board değişikliği kullanıcı tarafından daha sonra yapılacak.


============================================================
94. MEVCUT DOSYALARA DİKKAT
============================================================

Önce tüm mevcut source/header/CMake/prj.conf/app.overlay dosyalarını oku.

Backup dosyalarını silme:

CMakeLists.txt.bak
src/main.c.bak varsa


Mevcut yapıyı gereksiz yere bozma.


============================================================
95. UYGULAMA SIRASI
============================================================

Kodlamayı şu sırayla yap:

1. deos_icd.h
2. deos_types.h
3. deos_internal.h
4. endian helpers
5. CAN ID codec
6. message/frame codec
7. init/runtime state
8. handler registry
9. dispatcher
10. PING/PONG
11. CAN-FD TX
12. CAN-FD RX queue
13. CAN-FD RX thread
14. heartbeat
15. router infrastructure
16. sample application
17. tests
18. README
19. architecture documentation


Her adımda mimariyi temiz tut.


============================================================
96. SONUNDA RAPORLA
============================================================

İş bittikten sonra bana şunları ver:

1. Oluşturduğun yeni dosyalar
2. Değiştirdiğin dosyalar
3. Public API listesi
4. DEOS RX flow
5. DEOS TX flow
6. Dispatcher flow
7. PING/PONG flow
8. Application handler nasıl register edilir
9. Main router nasıl çalışacak
10. Şu an implement edilmeyen ICD detayları
11. Board CAN-FD incompatibility varsa açıkla
12. Build/test durumunu açıkla
13. Sonraki geliştirme adımlarını öner


============================================================
97. EN ÖNEMLİ TASARIM HEDEFİ
============================================================

Application geliştiricisinin mümkün olduğunca CAN-FD ve DEOS wire formatını
bilmesine gerek kalmasın.

Application şunu yazabilsin:

steering_set_target_angle(...)

veya:

deos_register_handler(...)

veya:

deos_send(...)


Ama şunlarla uğraşmasın:

- CAN ID bit shift
- CAN DLC
- protocol version byte
- sequence byte
- source field
- endian
- Ping ID serialization
- extended frame flags


Communication complexity DEOS Core içerisinde kapsüllensin.


============================================================
98. ÖZET FELSEFE
============================================================

Application:

"Ne yapmak istiyorum?"


DEOS Core:

"Bunu CAN-FD üzerinde DEOS protokolüne uygun şekilde nasıl iletirim?"


Control thread:

"Bu hedefe fiziksel sistemi nasıl götürürüm?"


Bu üç katmanı birbirine karıştırma.