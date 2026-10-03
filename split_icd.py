import re

with open("docs/DEOS_CANFD_ICD_v0_3_protocol_v1_1 (1).tex", "r", encoding="utf-8") as f:
    content = f.read()

# Splitting by \section
sections = re.split(r'\\section{', content)
preamble = sections[0]

def create_doc(filename, title, section_indices):
    doc_content = preamble.replace(r'\title{\textbf{DEOS CAN-FD Interface Control Document (ICD)}', r'\title{\textbf{' + title + r'}')
    for idx in section_indices:
        doc_content += r'\section{' + sections[idx]
    
    # ensure it ends with \end{document} if not present
    if r'\end{document}' not in doc_content:
        doc_content += r'\end{document}' + '\n'
        
    with open(f"docs/icd/{filename}", "w", encoding="utf-8") as f:
        f.write(doc_content)

# Core Architecture: 1, 2, 3, 4, 5, 6 (Namespaces, ID, Nodes, Classes, Service, Node-Service)
create_doc("ICD_Core_Architecture.tex", "DEOS Core Architecture \& Protocol Rules", [1, 2, 3, 4, 5, 6, 11, 12, 13, 17, 18, 19, 20, 21])

# Traction Node
create_doc("ICD_Traction_Node.tex", "DEOS Traction Node ICD", [7, 8, 9, 10]) # We'll put Command Namespace, Config, State, Data types here for now, but this needs filtering.

