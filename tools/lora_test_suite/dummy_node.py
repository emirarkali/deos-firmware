import serial
import struct
import time
import threading
import sys
from datetime import datetime

def tprint(*args, **kwargs):
    msg = " ".join(map(str, args))
    print(f"[{datetime.now().strftime('%H:%M:%S.%f')[:-3]}] {msg}", **kwargs)

# DEOS Node IDs
DEOS_NODE_GROUND_CONTROL = 0x02
DEOS_NODE_MAIN_STM32 = 0x03
DEOS_NODE_BMS_MAIN = 0x20
DEOS_NODE_BMS_AUX = 0x21
DEOS_NODE_BMS_GATEWAY = 0x30

# DEOS Priorities
DEOS_PRIO_STATUS = 3
DEOS_PRIO_NETWORK = 5

# DEOS Classes
DEOS_CLASS_REQUEST = 0x4
DEOS_CLASS_NETWORK = 0x7

# DEOS Services
DEOS_SERVICE_SYSTEM = 0x00
DEOS_SERVICE_BMS = 0x04

# DEOS Commands
DEOS_CMD_SYSTEM_PING = 0x08
DEOS_CMD_BMS_GET_STATUS = 0x01
DEOS_CMD_BMS_GET_CELL_VOLTAGES = 0x03
DEOS_CMD_BMS_GET_BALANCING = 0x07

# UART LORA Protocol Constants
LORA_MAGIC_1 = 0xAA
LORA_MAGIC_2 = 0x55
LORA_FLAG_NONE = 0x00
LORA_FLAG_ACK_REQ = 0x01
LORA_FLAG_IS_ACK = 0x02

def crc16_ccitt(crc, data):
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc <<= 1
        crc &= 0xFFFF
    return crc

class DeosLoraDummy:
    def __init__(self, port='/dev/ttyUSB0', baudrate=115200):
        try:
            self.ser = serial.Serial(port, baudrate, timeout=0.1)
            tprint(f"Baudrate {baudrate} ile {port} portuna bağlanıldı.")
        except Exception as e:
            tprint(f"Seri port hatası: {e}")
            sys.exit(1)
            
        self.seq = 0
        self.running = True

    def send_lora_frame(self, flag, payload_bytes):
        length = len(payload_bytes)
        # CRC hesaplaması Zephyr crc16_ccitt (0xFFFF seed) ile birebir aynı
        crc = crc16_ccitt(0xFFFF, struct.pack('<B', flag))
        crc = crc16_ccitt(crc, struct.pack('<B', length))
        if length > 0:
            crc = crc16_ccitt(crc, payload_bytes)
            
        header = struct.pack('<BBBB', LORA_MAGIC_1, LORA_MAGIC_2, flag, length)
        footer = struct.pack('<H', crc)
        
        frame = header + payload_bytes + footer
        self.ser.write(frame)
        self.ser.flush()

    def send_message(self, dest, prio, msg_class, service, cmd, payload=b''):
        # deos_message_t Header (8 byte - tamamen packed):
        # priority(1), class(1), service(1), dest(1), source(1), version(1), seq(1), cmd(1)
        header = struct.pack('<BBBBBBBB',
            prio, msg_class, service, dest, DEOS_NODE_GROUND_CONTROL, 0x11, self.seq, cmd
        )
        self.seq = (self.seq + 1) & 0xFF
        full_payload = header + payload
        self.send_lora_frame(LORA_FLAG_NONE, full_payload)

    def rx_thread(self):
        state = 0
        rx_flag = 0
        rx_len = 0
        rx_buf = bytearray()
        
        while self.running:
            try:
                c = self.ser.read(1)
            except:
                break
                
            if not c:
                continue
            b = c[0]
            
            # LoRa UART State Machine
            if state == 0:
                if b == LORA_MAGIC_1: state = 1
            elif state == 1:
                if b == LORA_MAGIC_2: state = 2
                elif b != LORA_MAGIC_1: state = 0
            elif state == 2:
                rx_flag = b
                state = 3
            elif state == 3:
                rx_len = b
                rx_buf = bytearray()
                if rx_len > 100: state = 0
                elif rx_len == 0: state = 5
                else: state = 4
            elif state == 4:
                rx_buf.append(b)
                if len(rx_buf) >= rx_len: state = 5
            elif state == 5:
                crc_lo = b
                state = 6
            elif state == 6:
                crc_hi = b
                rx_crc = crc_lo | (crc_hi << 8)
                
                # Gelen verinin CRC'sini doğrula
                calc = crc16_ccitt(0xFFFF, struct.pack('<B', rx_flag))
                calc = crc16_ccitt(calc, struct.pack('<B', rx_len))
                if rx_len > 0:
                    calc = crc16_ccitt(calc, bytes(rx_buf))
                    
                if calc == rx_crc:
                    self.handle_packet(rx_flag, bytes(rx_buf))
                else:
                    tprint(f"[!] CRC Hatası! Hesaplanan: {calc:04X} != Gelen: {rx_crc:04X}")
                state = 0

    def handle_packet(self, flag, payload):
        if flag == LORA_FLAG_IS_ACK:
            tprint("[+] ACK alındı.")
            return
            
        if len(payload) < 8:
            return
            
        prio, msg_class, service, dest, src, ver, seq, cmd = struct.unpack('<BBBBBBBB', payload[:8])
        msg_payload = payload[8:]
        
        if service == DEOS_SERVICE_SYSTEM and cmd == 0x09: # PONG
            tprint(f"[RX] PONG (Node 0x{src:02X})")
            
        elif service == DEOS_SERVICE_BMS and cmd == 0x02: # BMS_STATUS
            if len(msg_payload) >= 16:
                pack_v, pack_i, soc, temp, min_cv, max_cv, nom_v, cell_c, temp_c = struct.unpack('<HhHhHHHBB', msg_payload[:16])
                tprint(f"[RX] BMS STATUS (Node 0x{src:02X}): Pack V: {pack_v} mV, I: {pack_i} cA, SOC: {soc/10} %, Cells: {cell_c}")
                
        elif service == DEOS_SERVICE_BMS and cmd == 0x04: # CELL_VOLTAGE
            if len(msg_payload) >= 3:
                idx, vol = struct.unpack('<BH', msg_payload[:3])
                if idx == 0:
                    tprint(f"[RX] BMS CELL VOLTAGE (Node 0x{src:02X}): Liste Sonu.")
                else:
                    tprint(f"[RX] BMS CELL VOLTAGE (Node 0x{src:02X}): Hücre {idx} = {vol} mV")
                    
        elif service == DEOS_SERVICE_BMS and cmd == 0x08: # BALANCING
            if len(msg_payload) >= 2:
                idx, st = struct.unpack('<BB', msg_payload[:2])
                if idx == 0:
                    tprint(f"[RX] BMS BALANCING (Node 0x{src:02X}): Liste Sonu. Genel Durum = {st}")
                else:
                    tprint(f"[RX] BMS BALANCING (Node 0x{src:02X}): Hücre {idx} Durumu = {st}")
        else:
            tprint(f"[RX] Servis: 0x{service:02X}, Cmd: 0x{cmd:02X} (Gelen: 0x{src:02X})")

    def run(self):
        t = threading.Thread(target=self.rx_thread, daemon=True)
        t.start()
        
        loop_counter = 0
        try:
            tprint("--- LoRa Dummy Diagnostic Tool Başlatıldı ---")
            tprint("Tüm mesajlar art arda (100ms aralıklarla) gönderilecek...\n")
            while True:
                tprint("\n[TX] Ana Node (Main STM32) PING atılıyor...")
                ping_payload = struct.pack('<I', 0xDEADBEEF) # 4 bytelık ping_id
                self.send_message(DEOS_NODE_MAIN_STM32, DEOS_PRIO_NETWORK, DEOS_CLASS_NETWORK, DEOS_SERVICE_SYSTEM, DEOS_CMD_SYSTEM_PING, ping_payload)
                time.sleep(0.1)

                # tprint("[TX] BMS Node'larına PING atılıyor...")
                # self.send_message(DEOS_NODE_BMS_MAIN, DEOS_PRIO_NETWORK, DEOS_CLASS_NETWORK, DEOS_SERVICE_SYSTEM, DEOS_CMD_SYSTEM_PING)
                # time.sleep(0.1)

                # tprint("[TX] BMS GET_STATUS atılıyor...")
                # self.send_message(DEOS_NODE_BMS_MAIN, DEOS_PRIO_STATUS, DEOS_CLASS_REQUEST, DEOS_SERVICE_BMS, DEOS_CMD_BMS_GET_STATUS)
                # time.sleep(0.1)

                # tprint("[TX] BMS GET_CELL_VOLTAGES atılıyor...")
                # self.send_message(DEOS_NODE_BMS_MAIN, DEOS_PRIO_STATUS, DEOS_CLASS_REQUEST, DEOS_SERVICE_BMS, DEOS_CMD_BMS_GET_CELL_VOLTAGES)
                # time.sleep(0.1)

                # tprint("[TX] BMS GET_BALANCING atılıyor...")
                # self.send_message(DEOS_NODE_BMS_MAIN, DEOS_PRIO_STATUS, DEOS_CLASS_REQUEST, DEOS_SERVICE_BMS, DEOS_CMD_BMS_GET_BALANCING)
                
                tprint("Bekleniyor (2 saniye)...")
                time.sleep(2)
                
        except KeyboardInterrupt:
            tprint("\nÇıkış yapılıyor...")
            self.running = False
            self.ser.close()

if __name__ == '__main__':
    # Seri portunuzu buraya yazın (Örn: /dev/ttyUSB0 veya COM3)
    PORT = '/dev/ttyUSB0' 
    BAUDRATE = 115200 # STM32 115200'de dinliyor!
    
    dummy = DeosLoraDummy(port=PORT, baudrate=BAUDRATE)
    dummy.run()
