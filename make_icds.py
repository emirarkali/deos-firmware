import re

with open("docs/DEOS_CANFD_ICD_v0_3_protocol_v1_1 (1).tex", "r", encoding="utf-8") as f:
    content = f.read()

# Get preamble
preamble = content.split(r'\section{')[0]

def extract_section(title):
    pattern = r'\\section\{' + title + r'\}(.*?)(?=\\section\{|\\end\{document\})'
    m = re.search(pattern, content, re.DOTALL)
    return r'\section{' + title + r'}' + m.group(1) if m else ''

def extract_subsection(title):
    pattern = r'\\subsection\{' + title + r'\}(.*?)(?=\\subsection\{|\\section\{|\\end\{document\})'
    m = re.search(pattern, content, re.DOTALL)
    return r'\subsection{' + title + r'}' + m.group(1) if m else ''

def write_doc(filename, doc_title, body):
    head = preamble.replace(r'\title{\textbf{DEOS CAN-FD Interface Control Document (ICD)}', r'\title{\textbf{' + doc_title + r'}')
    
    # Remove \end{document} if it's somehow in body
    body = body.replace(r'\end{document}', '')
    
    full = head + body + r'\end{document}' + '\n'
    with open(f"docs/icd/{filename}", "w", encoding="utf-8") as f:
        f.write(full)

# 1. CORE ARCHITECTURE
core_body = ""
for s in ["Amaç ve Kapsam", "Namespace Hiyerarşisi", "CAN-FD Extended Identifier Yapısı", 
          "Node Address Registry", "Message Class Registry", "Service Registry", 
          "Node--Service İlişkisi", "CAN-FD DLC, Payload Uzunluğu ve BRS", 
          "REQUEST--RESPONSE ve CONFIG Ayrımı", "Response Altyapısı", 
          "Mantıksal Node Kimliği Kuralı", "İsimlendirme Standardı", "Özet Registry"]:
    core_body += extract_section(s)
write_doc("ICD_Core_Architecture.tex", "DEOS Core Architecture \\& Protocol Rules", core_body)

# 2. NETWORK & SYSTEM MANAGEMENT
sys_body = extract_subsection("SYSTEM Service Komutları")
sys_body += extract_section("PING--PONG Ağ Erişilebilirlik Mekanizması")
write_doc("ICD_Network_Management.tex", "DEOS Network \\& System Management", sys_body)

# 3. TRACTION NODE
tr_body = "\\section{Traction Node Commands}\n"
tr_body += extract_subsection("TRACTION Service Komutları")
write_doc("ICD_Traction.tex", "DEOS Traction Node ICD", tr_body)

# 4. STEERING & BRAKE NODES
st_body = "\\section{Steering \\& Brake Node Commands}\n"
st_body += extract_subsection("STEERING Service Komutları")
st_body += extract_subsection("BRAKE Service Komutları")
write_doc("ICD_Steering_Brake.tex", "DEOS Steering \\& Brake ICD", st_body)

# 5. DATA TYPES & COMMON PARAMS
dt_body = extract_section("Veri Tipleri ve Byte Sırası")
dt_body += extract_subsection("Ortak Parameter Command ID'leri")
dt_body += extract_subsection("Parameter ID Alanı")
dt_body += extract_section("State ve Mode Registry")
write_doc("ICD_Data_Types_Config.tex", "DEOS Data Types \\& Config", dt_body)

