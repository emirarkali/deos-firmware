Bu projede mevcut DEOS Core protocol stack içerisine
DEOS Fault Management mekanizmasını entegre et.

Çalışma dizini:

~/zephyrproject/applications/deos_core_proto

Amaç:

- DEOS Core içerisinde generic fault management mekanizması oluşturmak
- Common fault ID'lerini DEOS Core içerisinde tutmak
- Node-specific fault registry'lerini DEOS Core dışında bırakmak
- GET_FAULTS ve CLEAR_FAULTS protocol davranışlarını DEOS Core tarafından otomatik yürütmek
- Fault condition detection işini application/control tarafında bırakmak
- Fault listesinin CAN-FD üzerinden ICD'ye uygun şekilde gönderilmesini sağlamak


============================================================
1. MİMARİ PRENSİP
============================================================

Fault mekanizmasını ikiye ayır:

1) APPLICATION / CONTROL TARAFI

Application fiziksel veya yazılımsal hata condition'ını tespit eder.

Örnek:

- steering encoder communication lost
- motor overcurrent
- brake position invalid
- BMS measurement invalid

Application bu durumda DEOS Core'a yalnızca:

"şu fault oluştu"

veya:

"şu fault artık aktif değil"

bilgisini verir.

Application:

- GET_FAULTS protokolünü bilmemeli
- CLEAR_FAULTS protokolünü bilmemeli
- CAN-FD frame formatını bilmemeli
- fault response serialize etmemeli
- endian ile uğraşmamalı
- sequence/source/destination yönetmemeli


2) DEOS CORE TARAFI

DEOS Core:

- local fault kayıtlarını tutar
- occurrence count yönetir
- severity/state yönetir
- timestamp tutar
- GET_FAULTS işler
- CLEAR_FAULTS işler
- fault response oluşturur
- END_OF_FAULT_LIST gönderir
- fault response'ları 100 ms aralıklarla gönderir


Temel ayrım:

Fault detection
    = application responsibility

Fault storage + diagnostic protocol
    = DEOS Core responsibility


============================================================
2. FAULT DOKÜMANINA SADIK KAL
============================================================

Fault Management specification:

Her node kendi fault kayıt alanından sorumludur.

Global fault registry YOKTUR.

Fault identity:

Source Node ID + Fault ID

ikilisidir.

Örneğin:

STEERING / 0x0101

ile

TRACTION / 0x0101

aynı fault olmak zorunda değildir.


============================================================
3. DOSYA YAPISI
============================================================

Mevcut DEOS Core'a aşağıdaki modülü ekle:

lib/deos_core/
├── include/
│   └── deos/
│       ├── deos.h
│       ├── deos_icd.h
│       ├── deos_types.h
│       └── deos_fault.h
│
└── src/
    ├── deos_internal.h
    ├── deos_core.c
    ├── deos_codec.c
    ├── deos_rx.c
    ├── deos_tx.c
    ├── deos_dispatch.c
    ├── deos_network.c
    ├── deos_router.c
    └── deos_fault.c


Node-specific fault header'larını DEOS Core içine koyma.

Örnek node application yapısı:

apps/
├── steering/
│   └── include/
│       └── steering_faults.h
├── brake/
│   └── include/
│       └── brake_faults.h
├── traction/
│   └── include/
│       └── traction_faults.h
└── main_stm32/
    └── include/
        └── main_faults.h


============================================================
4. COMMON FAULT ID REGISTRY
============================================================

DEOS Core içerisinde common fault ID registry oluştur.

Fault ID 0x0000:

DEOS_FAULT_ID_END_OF_LIST

olarak reserved olsun.

Gerçek fault olarak kullanılmamalı.


Common fault ID'leri:

0x0001 = Internal Software Error
0x0002 = Watchdog Reset Detected
0x0003 = Invalid Configuration
0x0004 = Communication Timeout
0x0005 = RX Buffer / Queue Overflow
0x0006 = TX Failure
0x0007 = Protocol / Invalid Message Error
0x0008 = Local Storage Error


Örnek enum:

typedef enum
{
    DEOS_FAULT_ID_END_OF_LIST            = 0x0000,
    DEOS_FAULT_INTERNAL_SOFTWARE_ERROR    = 0x0001,
    DEOS_FAULT_WATCHDOG_RESET             = 0x0002,
    DEOS_FAULT_INVALID_CONFIGURATION      = 0x0003,
    DEOS_FAULT_COMMUNICATION_TIMEOUT      = 0x0004,
    DEOS_FAULT_RX_QUEUE_OVERFLOW          = 0x0005,
    DEOS_FAULT_TX_FAILURE                 = 0x0006,
    DEOS_FAULT_PROTOCOL_ERROR             = 0x0007,
    DEOS_FAULT_LOCAL_STORAGE_ERROR        = 0x0008
} deos_common_fault_id_t;


Node-specific fault ID'leri genellikle 0x0100 ve üzeri olmalı.

Fakat node-specific registry DEOS Core içinde tanımlanmayacak.


============================================================
5. NODE-SPECIFIC FAULT REGISTRY AYRIMI
============================================================

Örneğin Steering application kendi header'ında şunu tanımlayabilir:

typedef enum
{
    STEERING_FAULT_ENCODER_COMM_LOST  = 0x0100,
    STEERING_FAULT_POSITION_INVALID   = 0x0101,
    STEERING_FAULT_ANGLE_OUT_OF_RANGE = 0x0102,
    STEERING_FAULT_MOTOR_OVERCURRENT  = 0x0103,
    STEERING_FAULT_MOTOR_STALL        = 0x0104,
    STEERING_FAULT_RESPONSE_TIMEOUT   = 0x0105

} steering_fault_id_t;


Brake ayrı registry tutabilir.

Traction ayrı registry tutabilir.

Main STM32 ayrı registry tutabilir.

DEOS Core sadece uint16_t fault_id görmeli.

Core:

0x0103 = ne?

bilmek zorunda değildir.


============================================================
6. FAULT SEVERITY
============================================================

Fault severity enum oluştur:

typedef enum
{
    DEOS_FAULT_SEVERITY_INFO     = 0x01,
    DEOS_FAULT_SEVERITY_WARNING  = 0x02,
    DEOS_FAULT_SEVERITY_ERROR    = 0x03,
    DEOS_FAULT_SEVERITY_CRITICAL = 0x04

} deos_fault_severity_t;


============================================================
7. FAULT STATE
============================================================

typedef enum
{
    DEOS_FAULT_STATE_INACTIVE = 0x00,
    DEOS_FAULT_STATE_ACTIVE   = 0x01,
    DEOS_FAULT_STATE_LATCHED  = 0x02

} deos_fault_state_t;


Semantics:

ACTIVE:
fault condition şu anda mevcut.

INACTIVE:
fault daha önce oluşmuş fakat condition artık mevcut değil.

LATCHED:
condition kalkmış olsa bile fault CLEAR_FAULTS işlemine kadar tutulabilir.


============================================================
8. FAULT RECORD STRUCT
============================================================

Fault record:

typedef struct
{
    uint16_t fault_id;
    uint8_t severity;
    uint8_t state;
    uint16_t occurrence_count;
    uint32_t last_occurrence_ms;

} deos_fault_record_t;


İstersen severity/state alanlarını enum typed yapabilirsin.

Timestamp:

Zephyr uptime milliseconds.

Örneğin:

k_uptime_get_32()

veya mevcut Zephyr sürümünde uygun API.


============================================================
9. STORAGE MODEL
============================================================

İlk sürümde fault kayıtlarını RAM'de tut.

Flash / EEPROM persistence ekleme.

Static allocation kullan.

Örnek:

#define DEOS_MAX_FAULT_RECORDS 32

static deos_fault_record_t fault_table[DEOS_MAX_FAULT_RECORDS];


malloc YOK.

heap YOK.


Boş entry yönetimi için:

- separate used flag
veya
- fault_id == 0

yaklaşımı kullanılabilir.

Ama 0x0000 END_OF_LIST reserved olduğu için fault_id == 0 boş slot olarak
mantıklı olabilir.

Kod okunabilir olsun.


============================================================
10. PUBLIC FAULT API
============================================================

deos_fault.h içerisinde application'a sade API sağla.

Önerilen API:

int deos_fault_raise(
    uint16_t fault_id,
    deos_fault_severity_t severity);

int deos_fault_set_inactive(
    uint16_t fault_id);

int deos_fault_latch(
    uint16_t fault_id);

int deos_fault_clear(
    uint16_t fault_id);

int deos_fault_clear_all(void);

bool deos_fault_is_active(
    uint16_t fault_id);


Gerekirse:

const deos_fault_record_t *deos_fault_get(...)

public yapmak yerine internal tut.

Public API'yi gereksiz büyütme.


============================================================
11. FAULT RAISE DAVRANIŞI
============================================================

deos_fault_raise() çağrıldığında:

1. fault_id validation yap
2. fault_id == 0 reject et
3. severity validation yap
4. mevcut fault'u ara

Eğer fault daha önce yoksa:

- yeni record oluştur
- state = ACTIVE
- severity = verilen severity
- occurrence_count = 1
- last_occurrence_ms = current uptime

Eğer fault zaten varsa:

- state = ACTIVE
- severity gerekirse güncellenebilir
- occurrence_count++
- last_occurrence_ms güncellenir


Occurrence count overflow davranışı deterministic olsun.

uint16_t max değerinde saturate etmek wrapping'den daha güvenli olabilir.

Örneğin:

if count < UINT16_MAX:
    count++


============================================================
12. FAULT SET INACTIVE
============================================================

deos_fault_set_inactive(fault_id):

Fault varsa:

state = INACTIVE

yapsın.

Occurrence count değiştirmesin.

Timestamp değiştirmesi gerekmiyorsa değiştirme.

Fault yoksa:

-ENOENT

veya uygun error.


============================================================
13. FAULT LATCH
============================================================

deos_fault_latch(fault_id):

Mevcut fault:

state = LATCHED

yapsın.

Bu fonksiyon semantik olarak condition ortadan kalkmış olsa bile fault'un
stored kalması gerektiğini ifade eder.


============================================================
14. CLEAR SEMANTİĞİ
============================================================

CLEAR_FAULTS stored fault record'larını temizler.

Physical/application fault condition'ını çözmez.

Örneğin:

motor overcurrent halen devam ediyorsa:

CLEAR_FAULTS
    ->
fault record silinir
    ->
control loop tekrar condition görür
    ->
deos_fault_raise()
    ->
fault yeniden oluşur


Bu ayrımı comment ve documentation'da açık belirt.


============================================================
15. DIAGNOSTIC SERVICE
============================================================

Service:

DEOS_SERVICE_DIAGNOSTIC = 0x06


Relevant commands:

DEOS_CMD_DIAG_GET_FAULTS = 0x02

DEOS_CMD_DIAG_CLEAR_FAULTS = 0x03


Mevcut deos_icd.h içinde varsa duplicate oluşturma.

Yoksa ICD'ye uygun ekle.


============================================================
16. GET_FAULTS REQUEST
============================================================

GET_FAULTS request:

Message Class:
DEOS_CLASS_REQUEST

Service:
DEOS_SERVICE_DIAGNOSTIC

Command:
DEOS_CMD_DIAG_GET_FAULTS

Command-specific payload:

YOK


Request payload length:

0


Dispatcher geçerli local GET_FAULTS gördüğünde application handler çağırmadan
DEOS Fault Manager'a yönlendirsin.


============================================================
17. GET_FAULTS RESPONSE FORMAT
============================================================

Her fault AYRI CAN-FD frame içerisinde gönderilecek.

Response:

Message Class:
DEOS_CLASS_RESPONSE

Service:
DEOS_SERVICE_DIAGNOSTIC

Command:
DEOS_CMD_DIAG_GET_FAULTS

Source:

local node / fault owner

Destination:

GET_FAULTS request source


Command-specific payload formatı:

Offset 0:
uint16_t Fault ID

Offset 2:
uint8_t Severity

Offset 3:
uint8_t State

Offset 4:
uint16_t Occurrence Count

Offset 6:
uint32_t Last Occurrence Time [ms]


Command-specific payload total:

10 bytes


Little-Endian:

Fault ID
Occurrence Count
Timestamp


General DEOS frame payload:

Byte 0  Protocol Version
Byte 1  Sequence
Byte 2  GET_FAULTS
Byte 3-4 Fault ID
Byte 5 Severity
Byte 6 State
Byte 7-8 Occurrence Count
Byte 9-12 Timestamp


Semantic data length:

13 byte


CAN-FD DLC fiziksel olarak daha yüksek bir size'a yuvarlanıyorsa kalan padding
byte'ları sıfır olmalı.


============================================================
18. END OF FAULT LIST
============================================================

Fault listesi bittikten sonra END response gönder.

END:

Fault ID = 0x0000
Severity = 0x00
State = 0x00
Occurrence Count = 0x0000
Last Occurrence Time = 0x00000000


Bu frame:

Message Class = RESPONSE
Service = DIAGNOSTIC
Command = GET_FAULTS

olarak gönderilecek.


Node üzerinde hiç fault yoksa:

GET_FAULTS
    ->
doğrudan END_OF_FAULT_LIST

gönder.


============================================================
19. 100 MS RESPONSE ARALIĞI
============================================================

Ardışık fault response frame'leri arasında nominal:

100 ms

beklenmeli.

İlk response için 100 ms beklemek zorunlu değil.


ÇOK ÖNEMLİ:

DEOS RX thread içerisinde:

for(...)
{
    send();
    k_sleep(100ms);
}

YAPMA.


RX thread'i bloklama.


Bunun yerine Zephyr:

k_work_delayable

veya benzer non-blocking delayed work mekanizması kullan.


Önerilen akış:

GET_FAULTS geldi
        |
        v
fault transfer context oluştur
        |
        v
ilk fault hemen gönder
        |
        v
schedule delayed work 100 ms
        |
        v
sonraki fault
        |
        v
schedule 100 ms
        |
        ...
        |
        v
END_OF_FAULT_LIST


============================================================
20. FAULT TRANSFER CONTEXT
============================================================

Fault listesi transferi için static internal context oluştur.

Örneğin:

struct deos_fault_transfer
{
    bool active;
    deos_node_id_t destination;
    size_t current_index;
};


Gerekirse fault snapshot davranışını düşün.

Basit ve deterministic bir çözüm kullan.

GET_FAULTS sırasında fault table değişirse memory safety problemi oluşmamalı.

İlk versiyonda:

- transfer başlangıcında table lock
veya
- entry-by-entry safe read
veya
- snapshot copy

yaklaşımından basit olanını seç.

Heap kullanma.


============================================================
21. AYNI ANDA İKİ GET_FAULTS
============================================================

İlk sürümde yalnız bir aktif fault transfer desteklenebilir.

Eğer transfer devam ederken yeni GET_FAULTS gelirse:

-EBUSY

mantığı internal olabilir.

Fakat wire RESPONSE formatı exact olarak frozen değilse kafadan BUSY response
paketi oluşturma.

En azından logla ve mevcut transferi bozma.


============================================================
22. CLEAR_FAULTS REQUEST
============================================================

CLEAR_FAULTS:

Message Class:
DEOS_CLASS_COMMAND

Service:
DEOS_SERVICE_DIAGNOSTIC

Command:
DEOS_CMD_DIAG_CLEAR_FAULTS

Command-specific payload:

YOK


Geçerli local CLEAR_FAULTS geldiğinde:

deos_fault_clear_all()

çağır.


Fault transfer aktifken clear gelirse data race oluşmamasını sağla.


============================================================
23. FAULT TABLE CONCURRENCY
============================================================

Fault API farklı application thread'lerinden çağrılabilir.

Örneğin:

Steering Control Thread
Sensor Thread
DEOS RX Thread

aynı fault table'a erişebilir.


Bu nedenle fault table thread-safe olmalı.

Zephyr mutex veya uygun lock kullan.

ISR'dan deos_fault_raise() çağrılması gerekiyorsa mevcut API'nin thread-only
olduğunu document et.

İlk sürümde fault API'nin thread context'te kullanılacağını kabul etmek daha
temiz olabilir.


============================================================
24. RX QUEUE OVERFLOW ENTEGRASYONU
============================================================

Mevcut DEOS RX queue overflow olduğunda common fault oluşturmak mantıklı.

Örneğin:

DEOS_FAULT_RX_QUEUE_OVERFLOW

raise edilebilir.


Ancak dikkat:

CAN RX callback ISR/callback context içinde doğrudan mutex kullanan
deos_fault_raise() çağırma.


Bunun yerine:

- counter artır
veya
- deferred work flag
veya
- RX thread daha sonra fault raise etsin


ISR safety'yi bozma.


============================================================
25. TX FAILURE ENTEGRASYONU
============================================================

deos_send() / CAN-FD transmission failure durumunda:

DEOS_FAULT_TX_FAILURE

oluşturulabilir.


Ancak recursion oluşturma.

Örneğin fault response gönderirken TX failure oluştu:

TX failure
    ->
fault raise
    ->
fault gönder
    ->
TX failure
    ->
sonsuz döngü

OLMAMALI.


Fault raise local record update'tir.

Fault oluştu diye otomatik CAN-FD event broadcast etme.

Bu sayede recursion önlenir.


============================================================
26. PROTOCOL ERROR ENTEGRASYONU
============================================================

Invalid DEOS frame decode edildiğinde:

DEOS_FAULT_PROTOCOL_ERROR

oluşturmak isteğe bağlı olabilir.

Her malformed packet'ta occurrence_count artması flood oluşturabilir.

Basit rate policy veya direct count olabilir.

Şimdilik deterministic ve sade tut.


============================================================
27. NODE-SPECIFIC FAULT ÖRNEKLERİ
============================================================

Bu registry'leri DEOS Core içine EKLEME.

Ancak docs/example olarak gösterebilirsin.


Steering:

0x0100 Steering Encoder Communication Lost
0x0101 Steering Position Invalid
0x0102 Steering Angle Out of Range
0x0103 Steering Motor Overcurrent
0x0104 Steering Motor / Actuator Stall
0x0105 Steering Response Timeout


Brake:

0x0100 Brake Position Feedback Invalid
0x0101 Brake Actuator Overcurrent
0x0102 Brake Actuator Stall
0x0103 Brake Position Out of Range
0x0104 Brake Response Timeout


Traction:

0x0100 Motor / Inverter Communication Lost
0x0101 Motor Overcurrent
0x0102 Motor Overtemperature
0x0103 Speed Feedback Invalid
0x0104 Overspeed Detected
0x0105 Actuator / Motor Response Timeout


Main:

0x0100 CAN-FD Bus Off
0x0101 Ethernet Link / Communication Failure
0x0102 LoRa Communication Failure
0x0103 Router Queue Overflow
0x0104 Invalid / Unknown Route


Bunları node application header'larında tutma prensibini README/docs'ta anlat.


============================================================
28. BMS FAULT SOURCE KURALI
============================================================

BMS Gateway kendi fault'unda:

Source = DEOS_NODE_BMS_GATEWAY


Gateway BMS Main adına fault response gönderiyorsa:

Source = DEOS_NODE_BMS_MAIN


Gateway BMS Aux adına fault response gönderiyorsa:

Source = DEOS_NODE_BMS_AUX


Mevcut public deos_send() source'u local node yaptığı için proxy source gerekirse
internal trusted send API kullan.

Normal application source spoof edememeli.


============================================================
29. DISPATCHER ENTEGRASYONU
============================================================

Mevcut deos_dispatch.c içerisinde common diagnostic handler ekle.

Önerilen akış:

decoded message
      |
      v
destination check
      |
      v
common handler check
      |
      +-- NETWORK/SYSTEM
      |      |
      |      +-- PING/PONG
      |
      +-- DIAGNOSTIC
             |
             +-- GET_FAULTS
             +-- CLEAR_FAULTS
      |
      +-- registered application handler


GET_FAULTS ve CLEAR_FAULTS application handler registry'ye düşmemeli.


============================================================
30. PUBLIC HEADER AYRIMI
============================================================

deos.h:

ana public API


deos_fault.h:

fault management public API


Application isterse:

#include <deos/deos.h>
#include <deos/deos_fault.h>

kullanabilir.


İstersen deos.h deos_fault.h'ı include edebilir.

Ama header dependency temiz olsun.


============================================================
31. KOD STİLİ
============================================================

Kod:

- Zephyr uyumlu
- static allocation
- deterministic
- thread-safe
- malloc'sız
- readable
- minimum coupling

olsun.


============================================================
32. TESTLER
============================================================

Fault modülü için hardware-independent unit test yaz.

En az:

1.
fault_id = 0 reject


2.
new fault raise:
state ACTIVE
count 1


3.
same fault tekrar raise:
count 2
timestamp update


4.
set inactive


5.
latch


6.
clear single fault


7.
clear all


8.
MAX fault table full:
-ENOSPC


9.
occurrence_count UINT16_MAX üzerinde wrap yapmamalı


10.
fault response payload encode/decode:
10 byte


11.
END_OF_LIST payload:
tüm alanlar zero


12.
GET_FAULTS no fault:
yalnız END


13.
GET_FAULTS multiple faults:
fault1
100 ms
fault2
100 ms
END

Timing test zor ise state-machine unit test yap.


============================================================
33. DOCUMENTATION
============================================================

README veya docs/fault_management.md oluştur.

Açıkça anlat:

Application:
fault condition'ı tespit eder.

DEOS Core:
fault kaydını tutar ve protokol üzerinden paylaşır.


Örnek:

if (!encoder_ok)
{
    deos_fault_raise(
        STEERING_FAULT_ENCODER_COMM_LOST,
        DEOS_FAULT_SEVERITY_ERROR);
}


Condition düzeldiğinde:

deos_fault_set_inactive(
    STEERING_FAULT_ENCODER_COMM_LOST);


Node-specific registry:

steering_faults.h

içerisinde tutulur.


============================================================
34. ÖRNEK STEERING HEADER
============================================================

DEOS Core içine production header olarak koyma.

Documentation/sample içinde örnek göster:

#ifndef STEERING_FAULTS_H
#define STEERING_FAULTS_H

typedef enum
{
    STEERING_FAULT_ENCODER_COMM_LOST   = 0x0100,
    STEERING_FAULT_POSITION_INVALID    = 0x0101,
    STEERING_FAULT_ANGLE_OUT_OF_RANGE  = 0x0102,
    STEERING_FAULT_MOTOR_OVERCURRENT   = 0x0103,
    STEERING_FAULT_MOTOR_STALL         = 0x0104,
    STEERING_FAULT_RESPONSE_TIMEOUT    = 0x0105

} steering_fault_id_t;

#endif


============================================================
35. ÖNEMLİ SINIR
============================================================

DEOS Core node-specific fault isimlerini bilmeyecek.

Core'un görevi:

uint16_t fault_id

ile mekanizmayı yürütmek.


Şunu DEOS Core içine yapma:

switch(local_node)
{
    case STEERING:
        fault 0x0100 = encoder...
}


Böyle bir yapı İSTEMİYORUM.


============================================================
36. MEVCUT PROTOKOL DAVRANIŞINI BOZMA
============================================================

Mevcut:

- CAN-FD codec
- send
- receive
- dispatcher
- ping/pong
- handler registration
- routing

davranışlarını mümkün olduğunca bozma.

Fault Management yeni bir common DEOS service module olarak eklensin.


============================================================
37. THREAD / WORK MODEL
============================================================

Yeni dedicated sürekli çalışan fault thread oluşturma.

Fault API:
normal function calls

GET_FAULTS streaming:
k_work_delayable

kullan.


Bu daha hafif ve uygun.


============================================================
38. FAULT STORAGE RESET
============================================================

deos_init() sırasında fault manager initialization yapılsın.

Fault table temiz başlasın.

Persistence şu aşamada YOK.


============================================================
39. ERROR RETURN
============================================================

Consistent errno-style return kullan:

0
-EINVAL
-ENOENT
-ENOSPC
-EBUSY

vb.


============================================================
40. SOURCE / DESTINATION
============================================================

Normal fault response:

Source:
fault sahibi local node

Destination:
GET_FAULTS requester


Request source mutlaka response destination olarak kullanılmalı.


============================================================
41. PRIORITY
============================================================

Fault response priority ICD'de ayrı kesin normatif değer olarak belirtilmemişse
kafadan yeni protocol standardı oluşturma.

Mevcut DEOS priority registry içerisinden makul internal seçim gerekiyorsa
comment'te provisional olduğunu belirt.

Örneğin DEOS_PRIO_STATUS düşünülebilir.

Ancak bunu ICD tarafından zorunluymuş gibi belgeleme.


============================================================
42. CLEAR_FAULTS RESPONSE
============================================================

ICD CLEAR_FAULTS command davranışını tanımlıyor ancak zorunlu exact RESPONSE
formatı tanımlamıyorsa kendi response packet formatını uydurma.

Command'ı uygula.

Future response TODO bırak.


============================================================
43. FAULT RESPONSE DLC / PADDING
============================================================

Semantic data length:

13 byte.

CAN-FD DLC physical representation daha büyük size gerektiriyorsa:

padding bytes = 0

olmalı.

Mevcut codec bunu genel olarak zaten çözüyorsa duplicate logic yazma.


============================================================
44. IMPLEMENTATION SIRASI
============================================================

Şu sırayla çalış:

1. mevcut fault-related ICD definitions kontrol et
2. deos_fault.h oluştur
3. fault types/constants ekle
4. deos_fault.c local registry
5. locking
6. fault raise/inactive/latch/clear
7. payload encode helper
8. diagnostic dispatcher integration
9. GET_FAULTS transfer state
10. delayed work
11. END_OF_LIST
12. CLEAR_FAULTS
13. common communication faults integration
14. tests
15. documentation


============================================================
45. SON KONTROL
============================================================

İş bitince bana raporla:

1. Yeni dosyalar
2. Değiştirilen dosyalar
3. Fault public API
4. Fault table yapısı
5. Locking yaklaşımı
6. GET_FAULTS flow
7. CLEAR_FAULTS flow
8. 100 ms streaming nasıl çözüldü
9. Application / Core sorumluluk ayrımı
10. Node-specific registry neden Core dışında
11. Unit test sonuçları
12. Build sonucu
13. ICD'de hâlâ açık kalan fault-related noktalar


============================================================
46. YAPILMAYACAKLAR
============================================================

Node-specific registry'leri Core'a koyma.

Flash persistence ekleme.

malloc kullanma.

Fault için ayrı sonsuz-loop thread açma.

GET_FAULTS sırasında RX thread'i sleep ile bloklama.

Fault oluştu diye otomatik broadcast CAN mesajı gönderme.

TX failure -> fault response -> TX failure şeklinde recursion oluşturma.

ICD'de olmayan ACK/RESPONSE formatı uydurma.

Application'ın CAN-FD detaylarıyla uğraşmasını gerektirme.


============================================================
47. TEMEL FELSEFE
============================================================

Application:

"Şu hata oluştu."


DEOS Core:

"Tamam, kaydını tutuyorum."


Remote Node:

"Fault'larını gönder."


DEOS Core:

"Tamam, ICD formatında sırayla gönderiyorum."


Application:

"Fault listesi nasıl CAN-FD'ye çevriliyor?"

Bunu bilmek zorunda değil.