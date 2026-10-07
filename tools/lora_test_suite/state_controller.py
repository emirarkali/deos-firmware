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

# DEOS Message Priorities
DEOS_PRIO_CONTROL = 2

# DEOS Message Classes
DEOS_CLASS_COMMAND = 0x01
DEOS_CLASS_REQUEST = 0x04
DEOS_CLASS_RESPONSE = 0x05

# DEOS Services
DEOS_SERVICE_SYSTEM = 0x00
DEOS_SERVICE_DIAGNOSTIC = 0x06

# DEOS SYSTEM Commands
DEOS_CMD_SYSTEM_SET_STATE = 0x02
DEOS_CMD_SYSTEM_HEARTBEAT = 0x07

# DEOS DIAGNOSTIC Commands
DEOS_CMD_DIAG_GET_FAULTS = 0x02

FAULT_SEVERITY = {
    1: "INFO",
    2: "WARNING",
    3: "ERROR",
    4: "CRITICAL"
}

FAULT_STATE = {
    0: "INACTIVE",
    1: "ACTIVE",
    2: "LATCHED"
}

FAULT_NAMES = {
    0: "END_OF_LIST",
    1: "INTERNAL_SOFTWARE_ERROR",
    2: "WATCHDOG_RESET",
    3: "INVALID_CONFIGURATION",
    4: "COMMUNICATION_TIMEOUT",
    5: "RX_QUEUE_OVERFLOW",
    6: "TX_FAILURE",
    7: "PROTOCOL_ERROR",
    8: "LOCAL_STORAGE_ERROR"
}

# State Definitions (from deos_icd.h)
DEOS_STATE_INIT = 0x00
DEOS_STATE_STANDBY = 0x01
DEOS_STATE_READY = 0x02
DEOS_STATE_ACTIVE = 0x03
DEOS_STATE_CALIBRATING = 0x04
DEOS_STATE_SAFE = 0x05
DEOS_STATE_FAULT = 0x06
DEOS_STATE_UNKNOWN = 0xFF

STATE_NAMES = {
    DEOS_STATE_INIT: "INIT",
    DEOS_STATE_STANDBY: "STANDBY",
    DEOS_STATE_READY: "READY",
    DEOS_STATE_ACTIVE: "ACTIVE",
    DEOS_STATE_CALIBRATING: "CALIBRATING",
    DEOS_STATE_SAFE: "SAFE",
    DEOS_STATE_FAULT: "FAULT",
    DEOS_STATE_UNKNOWN: "UNKNOWN"
}

# UART LORA Protocol Constants
LORA_MAGIC_1 = 0xAA
LORA_MAGIC_2 = 0x55
LORA_FLAG_NONE = 0x00

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

class DeosStateController:
    def __init__(self, port='/dev/ttyUSB0', baudrate=115200):
        try:
            self.ser = serial.Serial(port, baudrate, timeout=0.1)
            tprint(f"Baudrate {baudrate} ile {port} portuna baglanildi.")
        except Exception as e:
            tprint(f"Seri port hatasi: {e}")
            sys.exit(1)
            
        self.seq = 0
        self.running = True

    def send_lora_frame(self, flag, payload_bytes):
        length = len(payload_bytes)
        crc = crc16_ccitt(0xFFFF, struct.pack('<B', flag))
        crc = crc16_ccitt(crc, struct.pack('<B', length))
        if length > 0:
            crc = crc16_ccitt(crc, payload_bytes)
            
        header = struct.pack('<BBBB', LORA_MAGIC_1, LORA_MAGIC_2, flag, length)
        footer = struct.pack('<H', crc)
        
        frame = header + payload_bytes + footer
        self.ser.write(frame)
        self.ser.flush()

    def set_state(self, dest_node, target_state):
        # deos_message_t Header
        # priority(1), class(1), service(1), dest(1), source(1), version(1), seq(1), cmd(1)
        header = struct.pack('<BBBBBBBB',
            DEOS_PRIO_CONTROL, DEOS_CLASS_COMMAND, DEOS_SERVICE_SYSTEM, 
            dest_node, DEOS_NODE_GROUND_CONTROL, 0x11, self.seq, DEOS_CMD_SYSTEM_SET_STATE
        )
        self.seq = (self.seq + 1) & 0xFF
        
        payload = struct.pack('<B', target_state) # 1 byte state
        full_payload = header + payload
        self.send_lora_frame(LORA_FLAG_NONE, full_payload)

    def get_faults(self, dest_node):
        header = struct.pack('<BBBBBBBB',
            DEOS_PRIO_CONTROL, DEOS_CLASS_REQUEST, DEOS_SERVICE_DIAGNOSTIC, 
            dest_node, DEOS_NODE_GROUND_CONTROL, 0x11, self.seq, DEOS_CMD_DIAG_GET_FAULTS
        )
        self.seq = (self.seq + 1) & 0xFF
        self.send_lora_frame(LORA_FLAG_NONE, header) # Empty payload for GET_FAULTS

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
                
                calc = crc16_ccitt(0xFFFF, struct.pack('<B', rx_flag))
                calc = crc16_ccitt(calc, struct.pack('<B', rx_len))
                if rx_len > 0:
                    calc = crc16_ccitt(calc, bytes(rx_buf))
                    
                if calc == rx_crc:
                    self.handle_packet(rx_flag, bytes(rx_buf))
                state = 0

    def handle_packet(self, flag, payload):
        if len(payload) < 8:
            return
            
        prio, msg_class, service, dest, src, ver, seq, cmd = struct.unpack('<BBBBBBBB', payload[:8])
        msg_payload = payload[8:]
        
        if service == DEOS_SERVICE_SYSTEM:
            if cmd == DEOS_CMD_SYSTEM_HEARTBEAT and len(msg_payload) >= 1:
                current_state = msg_payload[0]
                state_name = STATE_NAMES.get(current_state, "BILINMIYOR")
                # Ekrani Heartbeat ile bogmamak icin sadece degistiginde veya yavasca basabilirsiniz.
                # Ama anlik izlemek guzeldir:
                tprint(f"[HEARTBEAT] Node 0x{src:02X} -> State: {state_name} (0x{current_state:02X})")
                
            elif msg_class == DEOS_CLASS_COMMAND and cmd == DEOS_CMD_SYSTEM_SET_STATE:
                if len(msg_payload) >= 1:
                    new_state = msg_payload[0]
                    state_name = STATE_NAMES.get(new_state, "BILINMIYOR")
                    tprint(f"[BROADCAST] Node 0x{src:02X} yeni duruma gecti: {state_name} (0x{new_state:02X})")
                
            elif msg_class == DEOS_CLASS_RESPONSE and cmd == DEOS_CMD_SYSTEM_SET_STATE:
                if len(msg_payload) >= 1:
                    result = msg_payload[0]
                    res_str = "SUCCESS" if result == 0 else f"ERROR ({result})"
                    tprint(f"[RESPONSE] SET_STATE Cevabi (Node 0x{src:02X}): {res_str}")

        elif service == DEOS_SERVICE_DIAGNOSTIC:
            if msg_class == DEOS_CLASS_RESPONSE and cmd == DEOS_CMD_DIAG_GET_FAULTS:
                if len(msg_payload) >= 10:
                    fault_id, severity, state, count, time_ms = struct.unpack('<HBBHI', msg_payload[:10])
                    
                    if fault_id == 0x0000:
                        tprint(f"[FAULTS] --- Liste Sonu (Node 0x{src:02X}) ---")
                    else:
                        fname = FAULT_NAMES.get(fault_id, f"UNKNOWN(0x{fault_id:04X})")
                        sev_str = FAULT_SEVERITY.get(severity, f"UNK({severity})")
                        state_str = FAULT_STATE.get(state, f"UNK({state})")
                        
                        tprint(f"[FAULTS] Node 0x{src:02X} -> {fname} | Sev: {sev_str} | Durum: {state_str} | Kez: {count} | Son MS: {time_ms}")

    def run(self):
        t = threading.Thread(target=self.rx_thread, daemon=True)
        t.start()
        
        tprint("--- DEOS State Controller Baslatildi ---")
        tprint("Kullanabileceginiz State komutlari:")
        tprint("0 = INIT, 1 = STANDBY, 2 = READY, 3 = ACTIVE, 4 = CALIBRATING, 5 = SAFE, 6 = FAULT")
        tprint("f = GET FAULTS (Hatalari Listele)")
        tprint("Gondermek istediginiz numarayi veya harfi yazip ENTER'a basin. (Cikmak icin q)")
        
        try:
            while True:
                user_input = input().strip()
                if user_input.lower() == 'q':
                    break
                    
                if user_input.isdigit():
                    val = int(user_input)
                    if val in STATE_NAMES:
                        tprint(f"\n[TX] Ana Node (STM32) -> SET_STATE = {STATE_NAMES[val]} gonderiliyor...")
                        self.set_state(DEOS_NODE_MAIN_STM32, val)
                    else:
                        tprint("Gecersiz State numarasi!")
                elif user_input.lower() == 'f':
                    tprint(f"\n[TX] Ana Node (STM32) -> GET_FAULTS komutu gonderiliyor...")
                    self.get_faults(DEOS_NODE_MAIN_STM32)
                else:
                    tprint("Lutfen 0-6 arasi bir rakam veya 'f' girin.")
                    
        except KeyboardInterrupt:
            pass
            
        tprint("\nCikis yapiliyor...")
        self.running = False
        self.ser.close()

if __name__ == '__main__':
    # Seri portunuzu buraya yazin
    PORT = '/dev/ttyUSB0' 
    BAUDRATE = 115200 
    
    controller = DeosStateController(port=PORT, baudrate=BAUDRATE)
    controller.run()
