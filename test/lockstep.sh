#!/bin/sh
# Regression test for the OACK half of the lockstep rules.  TFTP is a
# strict stop-and-wait protocol: exactly one unacknowledged DATA packet
# may be in flight, and per RFC 2347 a server that answered a RRQ with
# an OACK must wait for the client's ACK 0 before sending DATA block 1.
#
# oack.sh checks the shape of the OACK and dupack.sh checks the resend
# behaviour, but dupack.sh uses a plain RRQ, so nothing covered what an
# option-negotiating client sees.  That is the path U-Boot and other
# bootloaders take, and getting it wrong desyncs the transfer: the
# unsolicited DATA 1 puts the stream a block ahead of the ACKs, the
# client starts re-ACKing the last block it liked, and a server that
# advances on every ACK streams past the gap until the client gives up.
#
# Small files still work, which is what makes it look like a size
# problem: the whole file fits in the one block sent before the desync.

if [ x"${srcdir}" = x ]; then
    srcdir=.
fi
. ${srcdir}/lib.sh

check_dep python3

# Ten blocks at the block size U-Boot asks for.
BLKSIZE=1468
dd if=/dev/urandom of="$DIR/big.bin" bs=$BLKSIZE count=10 2>/dev/null

print "Negotiating blksize, verifying the server stays in lockstep ..."

BLKSIZE=$BLKSIZE python3 - <<'EOF'
import os, socket, struct, sys

DATA, ACK, ERROR, OACK = 3, 4, 5, 6
blksize = os.environ["BLKSIZE"].encode()
srv = ("127.0.0.1", 69)

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

def recv(timeout=3):
    s.settimeout(timeout)
    try:
        pkt, peer = s.recvfrom(4096)
    except socket.timeout:
        return None, None, None
    op = struct.unpack(">H", pkt[:2])[0]
    if op == ERROR:
        print("server ERROR:", pkt[4:].split(b"\0")[0].decode("latin1"))
        sys.exit(1)
    blk = struct.unpack(">H", pkt[2:4])[0] if op in (DATA, ACK) else None
    return op, blk, peer

s.sendto(b"\x00\x01big.bin\x00octet\x00blksize\x00" + blksize + b"\x00", srv)

op, _, tid = recv()
if op != OACK:
    print("expected OACK, got opcode", op)
    sys.exit(1)
print("got OACK, deliberately not acknowledging it yet")

# RFC 2347: nothing may follow the OACK until we ACK block 0.  A server
# that sends DATA 1 here is already a block ahead of us.
op, blk, _ = recv(timeout=2)
if op is not None:
    print(f"FAIL: server sent opcode {op} block {blk} before our ACK 0")
    sys.exit(1)
print("server correctly waited for ACK 0")

s.sendto(struct.pack(">HH", ACK, 0), tid)
op, blk, _ = recv()
if op != DATA or blk != 1:
    print(f"expected DATA 1 after ACK 0, got opcode {op} block {blk}")
    sys.exit(1)

# One DATA in flight at a time: nothing more until we ACK block 1.
op, blk, _ = recv(timeout=2)
if op is not None:
    print(f"FAIL: server sent opcode {op} block {blk} before our ACK 1")
    sys.exit(1)
print("one block in flight at a time")

s.sendto(struct.pack(">HH", ACK, 1), tid)
op, blk, _ = recv()
if op != DATA or blk != 2:
    print(f"expected DATA 2 after ACK 1, got opcode {op} block {blk}")
    sys.exit(1)

# Simulate a lost DATA 2: re-ACK block 1.  The server must resend block
# 2, not treat the ACK as permission to send block 3.
s.sendto(struct.pack(">HH", ACK, 1), tid)
op, blk, _ = recv()
print("after stale ACK(1) on the OACK path, server sent block", blk)
if op != DATA or blk != 2:
    print("FAIL: server streamed past the gap instead of resending")
    sys.exit(1)

print("lockstep maintained across the whole exchange")
sys.exit(0)
EOF

[ $? -eq 0 ] && OK
FAIL
