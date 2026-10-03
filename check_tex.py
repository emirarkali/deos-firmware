import os, glob

files = glob.glob("docs/icd/*.tex")
for f in files:
    with open(f, "r", encoding="utf-8") as file:
        content = file.read()
    
    print(f"--- Checking {os.path.basename(f)} ---")
    
    if "\\begin{document}" not in content:
        print("ERROR: Missing \\begin{document}")
    if "\\end{document}" not in content:
        print("ERROR: Missing \\end{document}")
        
    # Check for empty sections
    if "\\section{}" in content or "\\subsection{}" in content:
        print("WARNING: Empty section found.")
        
    print(f"Lines: {len(content.splitlines())}")

