# Issue: CAN FD Kesme Fırtınasının (Interrupt Storm) UART'ı Tıkaması

## Sorunun Tanımı
LoRa modülü (UART) üzerinden gelen DEOS mesaj paketlerinin (PING, GET_STATUS vb.) büyük bir kısmının ana düğüm (main_node) tarafından sessizce çöpe atıldığı ve yalnızca belirli mesajların kırıntılarının ekranda görüldüğü (örn: `Cmd: 0x03`) tespit edildi. İlk başta UART RX fonksiyonlarında bir hata veya CRC uyumsuzluğu olduğundan şüphelenildi. 

## Kök Neden (Root Cause) Analizi
Hatanın kök nedeni yazılım kaynaklı değil, STM32'nin **FDCAN donanımı** ve yönlendirici (Router) mimarisindeki senkron (blocking) yapıydı. 

Olaylar zinciri şu şekilde gerçekleşti:
1. **Senkron Yönlendirme:** `UartLora::rx_thread`, gelen ilk paketi okuduktan hemen sonra hedef cihaza (örn: BMS) ulaştırmak için `deos_feed_message()` fonksiyonunu çağırdı. Bu fonksiyon doğrudan `deos_internal_canfd_tx()` üzerinden `can_send(..., K_MSEC(100))` metoduna bağlandı.
2. **Kopuk CAN Hattı:** Fiziksel CAN hattı bağlanmamış/boş olduğu için, gönderilen çerçeve (frame) diğer uçtan "ACK" alamadı.
3. **Kesme Fırtınası (Interrupt Storm):** STM32 FDCAN donanımı, ACK hatası alır almaz otomatik olarak saniyenin binde biri (mikrosaniye) hızında ardı ardına tekrar gönderim denemesi yaptı. Her başarısız deneme, işlemciyi felç eden yoğun bir **Error Interrupt (Kesme)** fırtınası başlattı.
4. **UART Overrun (Taşma):** İşlemci, CAN hatalarıyla meşgul olurken (starvation) UART üzerinden gelen 2. ve 3. paketlerin bytelarını zamanında okuyamadı. UART donanımsal FIFO'su doldu ve Overrun hatası vererek byteları düşürdü.
5. **Sessiz İptal:** Kaybolan bytelar nedeniyle UART çerçeve hizası (`0xAA 0x55`) kaydı. Çerçeve uzunluğu (Len) yanlış okunduğu için (örn: `> LORA_MAX_PAYLOAD`), paketler sessizce (CRC hatası dahi basılmadan) düşürüldü. FDCAN donanımı Bus-Off durumuna geçip sustuktan sonra denk gelen 4. mesaj ise (işlemci rahatladığı için) düzgün okundu.

## Çözüm (Professional Architecture Fix)
Çekirdek (`lib/deos_core`) kütüphanesindeki `deos_tx.c` modülü yeniden yazılarak **Senkron mimariden Asenkron mimariye** geçildi:
- `tx_msgq` adında bir mesaj kuyruğu (Message Queue) oluşturuldu.
- `deos_tx_thread_func` adında sadece CAN gönderimleriyle ilgilenen bağımsız (dedicated) bir iş parçacığı oluşturuldu.
- Router, CAN'e paket göndermek istediğinde artık doğrudan `can_send` ile donanımı sürmek ve 100ms bloke olmak yerine, paketi `K_NO_WAIT` ile kuyruğa fırlatıp kendi dinleme işlemine (UART RX) saliseler içinde geri dönecek hale getirildi. 

Bu sayede, fiziksel CAN hattı ne kadar büyük sorun yaşarsa yaşasın (Kısa devre, kopukluk, interrupt fırtınası), sistemin geri kalanı ve UART haberleşmesi bu durumdan zerre kadar etkilenmeden (non-blocking) çalışmaya devam etmektedir.
