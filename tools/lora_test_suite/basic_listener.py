import serial
import time
import sys

def listen(port='/dev/ttyUSB0', baudrate=115200):
    try:
        ser = serial.Serial(port, baudrate, timeout=1.0)
        print(f"[*] Dinleniyor: {port} @ {baudrate} baud...")
        print("[*] Durdurmak için Ctrl+C'ye basın.\n" + "="*50)
    except Exception as e:
        print(f"[!] Seri port açılamadı: {e}")
        sys.exit(1)

    try:
        while True:
            if ser.in_waiting > 0:
                raw_data = ser.read(ser.in_waiting)
                
                # Gelen veriyi Hex formatında stringe çevir
                hex_str = " ".join([f"{b:02X}" for b in raw_data])
                
                # Okunabilir (Printable) ASCII karakterlerini stringe çevir
                ascii_str = ""
                for b in raw_data:
                    if 32 <= b <= 126:
                        ascii_str += chr(b)
                    else:
                        ascii_str += "." # Okunamaz karakterler için nokta koy

                print(f"[{time.strftime('%H:%M:%S.%f')[:-3]}] HEX: {hex_str}")
                print(f"[{time.strftime('%H:%M:%S.%f')[:-3]}] ASC: {ascii_str}")
                print("-" * 50)
            else:
                time.sleep(0.01)
                
    except KeyboardInterrupt:
        print("\n[*] Dinleme sonlandırıldı.")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()

if __name__ == '__main__':
    port = '/dev/ttyUSB0' # Kendi portunuza göre değiştirin (Windows için 'COM3' vb.)
    baud = 115200
    
    # İsterseniz terminalden port ve baud rate verebilirsiniz: 
    # python3 basic_listener.py /dev/ttyUSB1 115200
    if len(sys.argv) > 1:
        port = sys.argv[1]
    if len(sys.argv) > 2:
        baud = int(sys.argv[2])
        
    listen(port, baud)
