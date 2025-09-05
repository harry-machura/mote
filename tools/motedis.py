#!/usr/bin/env python3
import sys, struct

names = {
    0:"HALT", 1:"PUSHI", 2:"LOADL", 3:"STOREL",
    4:"ADD", 5:"SUB", 6:"MUL", 7:"DIV",
    8:"JMP", 9:"JZ", 10:"CALL", 11:"LT", 12:"EQ",
    13:"DUP", 14:"DROP", 15:"SWAP", 16:"OVER",
    17:"GT", 18:"GE", 19:"LE", 20:"NE",
    21:"NOT", 22:"AND", 23:"OR",
    24:"CALLUSER", 25:"RET"
}

def rd_i32(buf, i=0):
    return struct.unpack_from("<i", buf, i)[0]

def disasm(code):
    ip = 0
    while ip < len(code):
        pc = ip
        op = code[ip]; ip += 1
        name = names.get(op, f"OP_{op}?")
        if op == 1:  # PUSHI
            val = rd_i32(code, ip); ip += 4
            print(f"{pc:04X}: {name} {val}")
        elif op in (8,9,24):  # JMP, JZ, CALLUSER
            addr = rd_i32(code, ip); ip += 4
            print(f"{pc:04X}: {name} {addr}")
        elif op in (2,3,10):  # LOADL, STOREL, CALL (native)
            idx = code[ip]; ip += 1
            print(f"{pc:04X}: {name} {idx}")
        else:
            print(f"{pc:04X}: {name}")

def main():
    if len(sys.argv) != 2:
        print("Usage: motedis.py file.bin")
        sys.exit(1)
    with open(sys.argv[1],"rb") as f:
        code = f.read()
    disasm(code)

if __name__=="__main__":
    main()
