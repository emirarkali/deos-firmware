import serial
import time
import sys
import argparse

def get_serial(port):
    try:
        # Configuration is always done at 9600 baud for EBYTE modules
        return serial.Serial(port, baudrate=9600, bytesize=8, parity='N', stopbits=1, timeout=1)
    except Exception as e:
        print(f"[!] Seri port hatasi: {e}")
        sys.exit(1)

def read_config(port):
    ser = get_serial(port)
    print(f"[*] {port} uzerinden modül ayarlari okunuyor...")
    
    ser.reset_input_buffer()
    cmd = bytes([0xC1, 0x00, 0x08])
    ser.write(cmd)
    ser.flush()
    time.sleep(0.1)
    
    resp = ser.read(32)
    if not resp:
        print("[!] Modülden cevap alinamadi! M0 ve M1 pinlerinin 1 (HIGH) oldugundan emin olun.")
    else:
        print(f"[+] RX (Raw): {resp.hex(' ')}")
        if len(resp) >= 8:
            print(f"    ADDH: 0x{resp[3]:02X}")
            print(f"    ADDL: 0x{resp[4]:02X}")
            print(f"    REG0: 0x{resp[5]:02X} (Baudrate/AirRate)")
            print(f"    REG1: 0x{resp[6]:02X} (Power)")
            print(f"    REG2: 0x{resp[7]:02X} (Channel)")
    ser.close()

def factory_reset(port):
    ser = get_serial(port)
    print(f"[*] {port} fabrika ayarlarina donduruluyor (CH23, 9600 Baud)...")
    
    cmd = bytes([
        0xC0, 0x00, 0x08,
        0x00, 0x00,       # ADDH, ADDL
        0x62,             # REG0: 9600, 8N1, 2.4 kbps
        0x00,             # REG1: 22 dBm
        0x17,             # REG2: CH23 = 873.125 MHz
        0x03,             # REG3 default
        0x00, 0x00        # CRYPT_H, CRYPT_L
    ])
    
    ser.write(cmd)
    ser.flush()
    time.sleep(0.3)
    resp = ser.read(32)
    print(f"[+] RX: {resp.hex(' ')}")
    print("[*] Islem tamam.")
    ser.close()

def set_channel(port, channel, baud_115200=True):
    ser = get_serial(port)
    print(f"[*] {port} uzerinden Kanal {channel} olarak ayarlaniyor...")
    
    # E22/E32 module channels typically: 850 + channel_number (MHz)
    # So CH23 = 873 MHz
    reg0 = 0xE2 if baud_115200 else 0x62
    
    cmd = bytes([
        0xC0, 0x02, 0x03, # Kalici yazma, REG0'dan basla, 3 byte yaz
        reg0,             # REG0 (115200 veya 9600 baud)
        0x00,             # REG1 (22 dBm)
        channel & 0xFF    # REG2 (Kanal no)
    ])
    
    ser.write(cmd)
    ser.flush()
    time.sleep(0.3)
    resp = ser.read(32)
    print(f"[+] RX: {resp.hex(' ')}")
    print(f"[*] Modül hizi {'115200' if baud_115200 else '9600'} baud ve Kanal {channel} yapildi.")
    ser.close()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="LoRa Donanim Konfigurasyon Araci (M0=1, M1=1 olmali)")
    parser.add_argument('--port', default='/dev/ttyUSB0', help='Seri Port (ornek: /dev/ttyUSB0)')
    parser.add_argument('action', choices=['read', 'reset', 'set_channel'], help='Yapilacak islem')
    parser.add_argument('--channel', type=int, default=23, help='Set channel islemi icin kanal numarasi (0-80)')
    parser.add_argument('--fast', action='store_true', help='Set channel yaparken UART hizini 115200 yap')
    
    args = parser.parse_args()
    
    if args.action == 'read':
        read_config(args.port)
    elif args.action == 'reset':
        factory_reset(args.port)
    elif args.action == 'set_channel':
        set_channel(args.port, args.channel, args.fast)
