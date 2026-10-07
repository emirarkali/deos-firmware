import serial
import time
import sys

def main():
    port = "/dev/ttyUSB0"
    baud_rate = 115200

    print("=======================================")
    print("      LORA PING-PONG YANSITICI         ")
    print("=======================================")
    
    try:
        ser = serial.Serial(port, baud_rate, timeout=1)
        print(f"[*] Bağlantı: {port} @ {baud_rate} bps")
        print("[*] PING mesajları bekleniyor... (Çıkış için Ctrl+C)\n")
        
        while True:
            if ser.in_waiting > 0:
                data = ser.readline()
                if data:
                    timestamp = time.strftime('%H:%M:%S')
                    try:
                        text = data.decode('utf-8').strip()
                        print(f"[{timestamp}] ALINDI: {text}")
                        
                        # Eğer gelen mesaj PING ise geri PONG gönder
                        if text.startswith("PING"):
                            # PING 123 -> PONG 123
                            packet_num = text.split(" ")[1] if " " in text else "?"
                            pong_msg = f"PONG {packet_num}\r\n"
                            ser.write(pong_msg.encode('utf-8'))
                            print(f"[{timestamp}] YANITLANDI: {pong_msg.strip()}")
                            print("-" * 40)
                            
                    except UnicodeDecodeError:
                        print(f"[{timestamp}] RAW ALINDI (HEX): {data.hex().upper()}")
                        
            time.sleep(0.01)
            
    except serial.SerialException as e:
        print(f"\n[HATA] Port açılamadı! '{port}' takılı mı?")
        print(f"Detay: {e}")
    except KeyboardInterrupt:
        print("\n\n[*] Dinleme sonlandırıldı.")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()

if __name__ == "__main__":
    main()
