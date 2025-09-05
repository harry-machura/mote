#!/usr/bin/env python3
import sys, struct

ops = {
    "HALT":0, "PUSHI":1, "LOADL":2, "STOREL":3,
    "ADD":4, "SUB":5, "MUL":6, "DIV":7,
    "JMP":8, "JZ":9, "CALL":10, "LT":11, "EQ":12,
    "DUP":13, "DROP":14, "SWAP":15, "OVER":16,
    "GT":17, "GE":18, "LE":19, "NE":20,
    "NOT":21, "AND":22, "OR":23,
    "CALLUSER":24, "RET":25
}

def emit32(out, v):
    out.extend(struct.pack("<i", v))

def assemble(src):
    out = bytearray()
    labels = {}
    fixups = []
    pc = 0
    lines = src.splitlines()
    for lineno,line in enumerate(lines,1):
        line=line.split(";")[0].strip()
        if not line: continue
        toks=line.replace(","," ").split()
        if toks[0].endswith(":"):
            labels[toks[0][:-1]] = len(out)
            toks=toks[1:]
            if not toks: continue
        if not toks: continue
        op=toks[0].upper()
        if op not in ops:
            print("Unknown op",op,"at line",lineno)
            sys.exit(1)
        code=ops[op]
        out.append(code)
        if op=="PUSHI":
            emit32(out,int(toks[1]))
        elif op in ("JMP","JZ","CALLUSER"):
            arg=toks[1]
            if arg.lstrip("-").isdigit():
                emit32(out,int(arg))
            else:
                fixups.append((len(out),arg))
                emit32(out,0)
        elif op in ("LOADL","STOREL","CALL"):
            out.append(int(toks[1]))
    for at,label in fixups:
        if label not in labels:
            print("Undefined label",label)
            sys.exit(1)
        struct.pack_into("<i",out,at,labels[label])
    return out

def main():
    if len(sys.argv)!=3:
        print("Usage: asm_min.py input.asm output.bin")
        sys.exit(1)
    with open(sys.argv[1]) as f: src=f.read()
    out=assemble(src)
    with open(sys.argv[2],"wb") as f: f.write(out)

if __name__=="__main__":
    main()
