# Issue: AUX Pinini GND'ye Zorlamak ve Bootloader Mesajı Alınması

## Sorunun Tanımı
LoRa modülü ile kablosuz haberleşme testleri sırasında, UART üzerinden sürekli olarak anlamsız 'C' (0x43) karakterleri ve zaman zaman `"ymodem_back:10 Ready!!! Wait loader data!!!"` gibi okunabilir string formatında Bootloader mesajları alındığı görüldü. Bu durum başlangıçta ortamdaki başka bir cihaz (3D yazıcı vb.) ile yaşanan bir kablosuz frekans/kanal çakışması olarak yorumlanmıştı.

## Kök Neden (Root Cause)
Yapılan donanımsal hata ayıklama sonucunda, hatanın **kablosuz frekans çakışması olmadığı**; tam aksine LoRa modülünün **AUX pininin yazılımsal veya donanımsal olarak yanlışlıkla GND'ye (Toprak) çekilmesi** olduğu tespit edilmiştir. 

Ebyte LoRa modüllerinin (veya benzeri UART-RF dönüştürücülerin) içindeki mikrodenetleyici, AUX pini dışarıdan zorla GND'ye çekildiğinde (kısa devre edildiğinde) kendini korumaya almakta veya donanımsal bir Firmware Update (YMODEM Bootloader) / Test moduna girmektedir. Modül bu moda girdiğinde, karşı taraftan yazılım bekleme sinyali olarak sürekli `'C'` karakteri basarak UART hattını felç etmektedir.

## Çözüm
1. **Donanımsal Düzeltme:** Modülün AUX pini GND'den kurtarılarak, olması gerektiği gibi STM32 üzerinde bir `GPIO_INPUT` (Giriş) pinine (D4 / PE14) bağlandı.
2. **Kalıcı Mod Ayarı:** M0 ve M1 pinleri yazılım üzerinden kontrol edilmek yerine, doğrudan donanımsal olarak (kablo ile) GND'ye bağlandı. Böylece modül kalıcı olarak Normal/Transparent moda sabitlendi.
3. **Yazılımsal Temizlik:** `main_node` ve `simple_uart_listener` uygulamalarındaki eski M0/M1 sürme (OUTPUT_INACTIVE) kodları silinerek, yerlerine AUX pinini dinleyen (INPUT) kodlar eklendi.

Bu düzeltmeler sonrası, "Bootloader" gürültüleri ve sahte "Framing Error" sorunları tamamen ortadan kalkmıştır.
