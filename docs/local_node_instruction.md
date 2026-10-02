Mevcut DEOS Core mimarisini, tek bir fiziksel STM32 üzerinde birden fazla bağımsız DEOS logical node identity host edebilecek şekilde genişlet.

ÖNEMLİ:
Bu çalışma BMS-specific olmamalı.
Core, BMS_MAIN / BMS_AUX / Daly / BMS Gateway gibi kavramları bilmemeli.
Sadece generic "multiple local logical nodes" altyapısı eklenecek.

Amaç:
- Tek CAN-FD interface
- Tek RX queue/thread
- Tek codec/parser
- Tek dispatcher
- Tek router
- Fakat birden fazla local DEOS node identity

Her local logical node dış dünyaya gerçek bağımsız node gibi görünmeli.

Örneğin ileride application tarafı:
- primary node = 0x30
- additional local node = 0x20
- additional local node = 0x21

register edebilmeli.

Ama Core bu ID'lerin ne anlama geldiğini bilmemeli.

============================================================
1. TEMEL PRENSİP
============================================================

Bir fiziksel MCU üzerinde birden fazla DEOS identity bulunabilir.

Bunlar:
- aynı CAN controller'ı
- aynı RX thread'i
- aynı codec'i
- aynı dispatcher'ı

paylaşabilir.

Ama DEOS protocol açısından bağımsız node gibi davranmalıdır.

Özellikle:

PING -> node X

gelirse cevap:

PONG Source = node X

olmalı.

Başka local node bu node adına cevap vermemeli.

"Kimse kimsenin yerine konuşamaz" prensibini koru.

============================================================
2. PRIMARY NODE + ADDITIONAL LOCAL NODES
============================================================

Mevcut `deos_init()` primary/local node ile çalışmaya devam etsin.

Config'e generic bir capability flag ekle:

örnek:

bool hosted_nodes_enabled;

veya daha iyi isim bulursan kullan.

Ama BMS-specific isim kullanma.

Örnek:

struct deos_config {
    deos_node_id_t node_id;
    const struct device *can_dev;
    bool router_enabled;
    bool hosted_nodes_enabled;
};

Normal node:

hosted_nodes_enabled = false

Birden fazla local logical node host eden cihaz:

hosted_nodes_enabled = true

============================================================
3. LOCAL NODE REGISTRY
============================================================

Core içerisinde additional local node registry oluştur.

Public API önerisi:

int deos_register_local_node(deos_node_id_t node_id);

Bu fonksiyon:
- node ID valid mi kontrol etsin
- primary node ile duplicate olmasın
- registry'de duplicate olmasın
- capacity doluysa -ENOSPC dönsün
- hosted_nodes_enabled false ise -ENOTSUP veya uygun hata dönsün

Static allocation kullan.
malloc kullanma.

Örnek internal limit:

#define DEOS_MAX_LOCAL_NODES 4

veya uygun küçük bir configurable değer.

============================================================
4. LOCAL NODE CONTEXT
============================================================

Additional local node sadece node_id listesi olmasın.

Her local identity için minimum bağımsız context oluştur.

Öneri:

struct deos_local_node_context {
    deos_node_id_t node_id;
    uint8_t tx_sequence;
    bool in_use;
};

İlk aşamada gereksiz field ekleme.

Ama mimari şu şekilde genişletilebilir olmalı:
- fault context
- state
- mode
- node-specific protocol state

Şimdilik minimum implementasyon yap.

============================================================
5. PRIMARY NODE DA AYNI MODELDE DÜŞÜNÜLEBİLİR
============================================================

Mümkünse primary node için de aynı local-node lookup altyapısını kullan.

Örneğin internal helper:

bool deos_is_local_node(deos_node_id_t node_id);

Bu:
- primary node ise true
- registered additional local node ise true
- aksi halde false

dönsün.

Dispatcher doğrudan primary_node comparison yapmaktansa mümkün olduğunca bu helper üzerinden çalışsın.

============================================================
6. ROUTER MANTIĞI
============================================================

Mevcut routing sırasını düzelt.

Yanlış yaklaşım:

if destination != primary_node:
    if router_enabled:
        route

Bu hosted local node'ları yanlışlıkla route eder.

Doğru sıra:

1. Broadcast mı?
2. Destination local node mu?
   - primary node
   - registered additional local node
3. Local ise Core/application tarafından işle
4. Local değil ve router_enabled ise route et
5. Aksi halde drop

Unicast için kavramsal akış:

if (deos_is_local_node(msg->destination)) {
    deos_dispatch_local(msg);
}
else if (router_enabled) {
    deos_route(msg);
}
else {
    drop;
}

Hosted local node kontrolü router'dan ÖNCE yapılmalı.

============================================================
7. APPLICATION MODELİNİ DEĞİŞTİRME
============================================================

ÇOK ÖNEMLİ:

Hosted local node için ayrı bir:
- endpoint handler modeli
- özel application pipeline
- özel service switch
- özel parser

oluşturma.

Normal application handler sistemi nasıl çalışıyorsa hosted local node da aynısını kullanmalı.

Yani:
- aynı decoded `deos_message_t`
- aynı class/service/command mantığı
- aynı dispatcher
- aynı callback tipi

kullanılmalı.

============================================================
8. HANDLER REGISTRY NODE-AWARE OLMALI
============================================================

Mevcut handler key muhtemelen:

class + service + command

şeklinde.

Bunu:

local_node_id + class + service + command

şeklinde genişlet.

Public API olarak yeni fonksiyon ekle:

int deos_register_handler_for_node(
    deos_node_id_t local_node,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    deos_message_handler_t handler,
    void *user_data
);

Bu sadece REGISTERED LOCAL NODE için çalışmalı.

Eğer local_node:
- primary node
- veya registered additional local node

değilse:
-ENOENT / -EPERM / uygun hata dön.

Mevcut:

deos_register_handler(...)

API'sini bozma.

Onu primary node için wrapper yap:

deos_register_handler(...)
    ->
deos_register_handler_for_node(primary_node, ...)

Backward compatibility korunmalı.

============================================================
9. SEND FROM LOCAL NODE
============================================================

Yeni generic API ekle:

int deos_send_from_node(
    deos_node_id_t source_node,
    deos_node_id_t destination,
    deos_priority_t priority,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    const void *payload,
    size_t payload_len
);

Bu fonksiyon sadece local registered identity adına göndermeye izin versin.

Kontrol:

if (!deos_is_local_node(source_node))
    return -EPERM;

Application istediği rastgele Source ID'yi spoof edememeli.

Mevcut:

deos_send(...)

API'sini bozma.

Onu wrapper yap:

deos_send(...)
    ->
deos_send_from_node(primary_node, ...)

============================================================
10. SEQUENCE COUNTER NODE-SPECIFIC OLMALI
============================================================

Her local logical node kendi TX sequence counter'ına sahip olmalı.

Örneğin:

primary node sequence = 12
local node A sequence = 41
local node B sequence = 3

Bir node adına mesaj gönderilince sadece o node'un sequence counter'ı artsın.

Tek global sequence counter kullanma.

Bu, logical node'ların bağımsız görünmesi için önemli.

============================================================
11. PING / PONG
============================================================

Mevcut Core common behavior korunmalı.

Ama destination local additional node ise de çalışmalı.

Örnek:

PING:
Source      = X
Destination = LOCAL_NODE_A

Core:
- LOCAL_NODE_A'nın local olduğunu görür
- PONG üretir

PONG:
Source      = LOCAL_NODE_A
Destination = X

olmalı.

Primary node adına cevap verilmemeli.

Bunun için common network handler hangi local identity hedeflendiyse onu bilmeli.

Gerekirse dispatch fonksiyonlarına target local node context geçir.

============================================================
12. COMMON CORE HANDLERS
============================================================

PING/PONG gibi Core-owned protocol behavior'larda target identity korunmalı.

Aynı prensip ileride:
- fault management
- system service
- diagnostic

gibi Core common behavior için de geçerli olmalı.

Ancak bu değişiklikte gereksiz yere tüm modülleri yeniden tasarlama.

Mimariyi multi-local-node compatible hale getir.

============================================================
13. FAULT MANAGEMENT
============================================================

Mevcut fault manager primary node odaklıysa bu değişiklikte dikkatli ol.

Minimum hedef:

- mevcut fault API'lerini bozma
- primary node fault behavior aynen çalışsın

Ama architecture'ı ileride node-specific fault context destekleyecek şekilde hazırlamak tercih edilir.

Eğer güvenli ve temiz şekilde yapılabiliyorsa:

deos_fault_raise_for_node(
    deos_node_id_t local_node,
    uint16_t fault_id,
    deos_fault_severity_t severity
);

gibi generic API eklenebilir.

Mevcut:

deos_fault_raise(...)

primary node wrapper olabilir.

Ancak bu değişiklik çok büyük refactor gerektiriyorsa:
- fault manager'ı bozma
- TODO/documentation bırak
- local node context tasarımını buna uygun hazırla

BMS-specific fault ekleme.

============================================================
14. HARDWARE FILTER
============================================================

Mevcut hardware filtering mantığını bu mimariyle uyumlu tut.

Router enabled ise:
- broad extended filter
- tüm gerekli frame'leri görebilmeli

router_enabled == false ve hosted nodes yoksa:
- primary local destination
- broadcast

kabul edilebilir.

router_enabled == false fakat hosted nodes varsa:
- primary local node
- registered additional local node'lar
- broadcast

için hardware filters kurulmalı.

Ancak local nodes `deos_start()` öncesinde register edilmelidir.

Bu nedenle lifecycle şu şekilde document edilebilir:

deos_init()
deos_register_local_node(...)
deos_register_handler_for_node(...)
deos_start()

Hardware filters start sırasında kurulursa registered local node listesi kullanılabilir.

Mevcut lifecycle buna uygun değilse minimum değişiklikle düzenle.

============================================================
15. REGISTER ORDER / LIFECYCLE
============================================================

Beklenen kullanım:

deos_init(&config);

deos_register_local_node(NODE_A);
deos_register_local_node(NODE_B);

deos_register_handler_for_node(...);

deos_start();

`deos_start()` sonrasında yeni local node register edilmesine izin verme.

Örneğin:
-EBUSY

dön.

Bu hardware filter ve runtime state açısından deterministic davranış sağlar.

============================================================
16. BROADCAST
============================================================

Broadcast behavior'ı dikkatli ele al.

Broadcast bir physical node'da host edilen tüm logical node'lara otomatik olarak application callback çağırmalı mı konusu protokolde kesin değilse kafadan davranış uydurma.

Mevcut broadcast behavior'ı bozma.

Sadece code path'te hosted local nodes ile çakışma yaratmamasını sağla.

Gerekirse TODO/documentation bırak.

============================================================
17. PARSER
============================================================

Tek parser kullanılmalı.

Her hosted node için ayrı CAN parser oluşturma.

Akış:

CAN RX
  ->
decode once
  ->
deos_message_t
  ->
destination lookup
  ->
appropriate local logical node
  ->
same dispatcher/application system

Hosted node'un tek farkı:
mesaj ona parse edilmiş olarak teslim edilir.

============================================================
18. BMS-SPECIFIC KOD YAZMA
============================================================

Aşağıdakileri production Core'a EKLEME:

DEOS_NODE_BMS_MAIN
DEOS_NODE_BMS_AUX
DEOS_NODE_BMS_GATEWAY

hard-coded branch'leri.

Daly logic.

BMS command-specific logic.

Core generic kalmalı.

BMS application daha sonra main.c içinde:

deos_register_local_node(...)

çağıracak.

============================================================
19. ÖRNEK KULLANIM DOKÜMANTASYONU
============================================================

Generic örnek ver:

struct deos_config cfg = {
    .node_id = DEOS_NODE_X,
    .can_dev = can_dev,
    .router_enabled = true,
    .hosted_nodes_enabled = true,
};

deos_init(&cfg);

deos_register_local_node(DEOS_NODE_Y);
deos_register_local_node(DEOS_NODE_Z);

deos_register_handler_for_node(
    DEOS_NODE_Y,
    ...);

deos_start();

Mesaj gönderme:

deos_send_from_node(
    DEOS_NODE_Y,
    destination,
    ...);

Ama example'da mümkünse BMS isimleri kullanma.

============================================================
20. BACKWARD COMPATIBILITY
============================================================

Mevcut tek-node uygulamalar çalışmaya devam etmeli.

Şu API'leri bozma:

deos_init()
deos_start()
deos_send()
deos_register_handler()
deos_send_ping()
fault API'leri

Yeni functionality additive olsun.

Primary node kullanan eski application kodu mümkün olduğunca değişmeden build olmalı.

============================================================
21. TESTLER
============================================================

En az şu testleri ekle:

1. primary node local olarak tanınır

2. registered additional node local olarak tanınır

3. unregistered node local değildir

4. duplicate local-node registration reject edilir

5. registry capacity overflow -> -ENOSPC

6. hosted_nodes_enabled false iken additional registration reject edilir

7. deos_send_from_node registered local node ile çalışır

8. deos_send_from_node unregistered source ile reject edilir

9. sequence counters local node başına bağımsız ilerler

10. handler registry aynı class/service/command için iki farklı local node'a farklı handler register edebilir

11. destination local additional node olduğunda router çağrılmaz

12. destination local değil + router enabled -> router path

13. destination local değil + router disabled -> drop

14. PING -> hosted local node:
PONG source hosted node olmalı

15. existing primary-node behavior bozulmamalı

============================================================
22. CODE QUALITY
============================================================

- Zephyr uyumlu
- static allocation
- malloc yok
- deterministic
- thread-safe
- minimum coupling
- generic naming
- BMS-specific knowledge yok
- existing architecture mümkün olduğunca korunmalı

============================================================
23. İSİMLENDİRME
============================================================

"endpoint" yerine mümkünse:

local node
hosted local node
logical local node

terminolojisini kullan.

Çünkü bunlar proxy değil; protocol açısından gerçek bağımsız DEOS node identity'leri.

============================================================
24. SON RAPOR
============================================================

İş bitince şunları raporla:

1. Yeni dosyalar
2. Değiştirilen dosyalar
3. Yeni public API'ler
4. Local node registry yapısı
5. Sequence counter yapısı
6. Handler registry nasıl node-aware oldu
7. deos_send_from_node nasıl doğrulama yapıyor
8. Router decision order
9. PING/PONG hosted node behavior
10. Hardware filter lifecycle
11. Backward compatibility durumu
12. Build sonucu
13. Test sonucu
14. Henüz future work olarak kalan multi-node fault/state konuları

En önemli hedef:

Core BMS'ten habersiz kalmalı.

Core sadece şunu bilsin:

"Bu physical DEOS runtime birden fazla local logical node identity host edebilir."

Bu local node'ların hangi gerçek cihazları temsil ettiği tamamen application katmanının sorumluluğudur.