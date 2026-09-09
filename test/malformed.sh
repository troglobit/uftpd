#!/bin/sh
# Malformed TFTP requests must draw an ERROR and leave the server
# serving, rather than ending the session:
#
#   RRQ/WRQ/DATA opcode only   packet shorter than the header its opcode
#                              implies, so the payload length is bogus
#   unterminated filename      an option walk with no terminator to stop on
#   blksize + trailing option  alloc_buf() reallocates the buffer the
#                              option walk is still reading from
#   huge blksize               an unbounded session buffer from one packet
#   DATA with no transfer      fwrite() with ctrl->fp still NULL
#
# A dead session answers nothing, so the check is that an ERROR comes
# back, not whether the parent survived -- it forks per session and
# survives either way.

if [ x"${srcdir}" = x ]; then
    srcdir=.
fi
. ${srcdir}/lib.sh

check_dep python3

print "Sending malformed TFTP requests, expecting ERROR not a crash ..."

python3 - <<'EOF'
import socket, struct, sys

DATA, ERROR, OACK = 3, 5, 6
NUL = b"\x00"
srv = ("127.0.0.1", 69)
fail = 0

# Keep every socket alive for the whole run.  A closed one frees its
# ephemeral port for the next probe to reuse, and a lingering session we
# never acknowledged can then land its retransmit on the wrong probe.
socks = []

def send(pkt, timeout=3):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(timeout)
    socks.append(s)
    s.sendto(pkt, srv)
    try:
        rsp, peer = s.recvfrom(4096)
    except socket.timeout:
        return None
    # An OACK opens a session that waits for our ACK 0; end it rather
    # than leave it retransmitting into the rest of the test.
    if rsp[:2] == struct.pack(">H", OACK):
        s.sendto(struct.pack(">HH", ERROR, 0) + b"done" + NUL, peer)
    return rsp

def req(op, *fields):
    return struct.pack(">H", op) + b"".join(f.encode() + NUL for f in fields)

def opcode(rsp):
    return struct.unpack(">H", rsp[:2])[0] if rsp and len(rsp) >= 2 else None

# Packets that must be rejected with an ERROR, not a dead session.
for name, pkt in [
    ("RRQ, opcode only",       b"\x00\x01"),
    ("WRQ, opcode only",       b"\x00\x02"),
    ("DATA, opcode only",      b"\x00\x03"),
    ("RRQ, unterminated file", b"\x00\x01testfile.txt"),
    ("DATA, no transfer open", struct.pack(">HH", DATA, 0)),
]:
    rsp = send(pkt)
    if rsp is None:
        print(f"  {name:<26} no reply, session died")
        fail = 1
    elif opcode(rsp) != ERROR:
        print(f"  {name:<26} expected ERROR, got {rsp[:24]!r}")
        fail = 1
    else:
        print(f"  {name:<26} ERROR: {rsp[4:].split(NUL)[0].decode('latin1')}")

# Option handling that must not touch freed or unbounded memory.
rsp = send(req(1, "testfile.txt", "octet", "blksize", "1468", "timeout", "5"))
if opcode(rsp) != OACK:
    print(f"  {'blksize + trailing opt':<26} expected OACK, got {rsp!r}")
    fail = 1
else:
    print(f"  {'blksize + trailing opt':<26} OACK: {b' '.join(rsp[2:].split(NUL)).decode('latin1').strip()}")

rsp = send(req(1, "testfile.txt", "octet", "blksize", "999999999999"))
if opcode(rsp) != OACK:
    print(f"  {'huge blksize':<26} expected OACK, got {rsp!r}")
    fail = 1
else:
    sz = int(rsp[2:].split(NUL)[1])
    print(f"  {'huge blksize':<26} clamped to {sz}")
    if sz > 65464:
        print("      not clamped to the RFC 2348 maximum")
        fail = 1

# The server must still serve a normal request afterwards.
rsp = send(req(1, "testfile.txt", "octet"))
if opcode(rsp) != DATA:
    print(f"  {'control, plain RRQ':<26} server no longer serving, got {rsp!r}")
    fail = 1
else:
    print(f"  {'control, plain RRQ':<26} DATA block 1, {len(rsp) - 4} bytes")

sys.exit(fail)
EOF

[ $? -eq 0 ] && OK
FAIL
