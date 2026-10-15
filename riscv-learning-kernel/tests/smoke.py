#!/usr/bin/env python3
"""Host-side integration test: boot the real ELF with OpenSBI and UART I/O.

Python is only a test runner; none of this code is linked into the kernel.
"""
import os
from pathlib import Path
import select
import subprocess
import sys
import time


def main():
    qemu, elf = sys.argv[1:]
    proc = subprocess.Popen(
        [qemu, "-machine", "virt", "-accel", "tcg", "-m", "128M",
         "-smp", "1", "-bios", "default", "-kernel", elf,
         "-nographic", "-nic", "none"],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        bufsize=0,
    )
    transcript = bytearray()
    pending = bytearray()
    prompt = b"kernel> "
    count = 0

    def expect_prompt():
        deadline = time.monotonic() + 10
        while prompt not in pending:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise AssertionError(f"Timeout waiting for prompt: {bytes(pending)!r}")
            ready, _, _ = select.select([proc.stdout], [], [], remaining)
            if not ready:
                continue
            chunk = os.read(proc.stdout.fileno(), 4096)
            if not chunk:
                raise AssertionError(f"QEMU exited early: {bytes(pending)!r}")
            transcript.extend(chunk)
            pending.extend(chunk)
        end = pending.index(prompt) + len(prompt)
        result = bytes(pending[:end])
        del pending[:end]
        return result

    def exchange(label, command, expected):
        nonlocal count
        # Slowly feed the emulated serial port; avoid overrunning its small FIFO.
        for c in command:
            proc.stdin.write(bytes([c]))
            time.sleep(0.001)
        actual = expect_prompt()
        if actual != expected + prompt:
            raise AssertionError(f"{label}: expected {expected + prompt!r}, got {actual!r}")
        count += 1
        print(f"PASS {label}")

    try:
        boot = expect_prompt()
        assert b"OpenSBI" in boot, boot
        assert b"Domain0 Next Mode" in boot and b"S-mode" in boot, boot
        assert b"Hello Kernel\r\nRISC-V S-mode shell. Type help.\r\n" in boot, boot
        count += 1
        print("PASS OpenSBI -> S-mode -> Hello Kernel -> shell")
        exchange("help", b"help\r", b"help\r\nhelp         - show commands\r\n"
                 b"echo [text]  - print text\r\nclear        - clear screen (ANSI)\r\n")
        exchange("echo hello", b"echo hello\r", b"echo hello\r\nhello\r\n")
        exchange("clear ANSI sequence", b"clear\r", b"clear\r\n\x1b[2J\x1b[H")
        exchange("empty echo (LF)", b"echo\n", b"echo\r\n\r\n")
        exchange("empty line", b"\r", b"\r\n")
        exchange("spaces and tabs", b" \techo\t alpha  beta\r",
                 b"  echo  alpha  beta\r\nalpha  beta\r\n")
        exchange("unknown command", b"nope\r", b"nope\r\nUnknown command: nope\r\n")
        exchange("DEL", b"echo hellx\x7fo\r", b"echo hellx\b \bo\r\nhello\r\n")
        exchange("backspace at start and middle", b"\x08echo ax\x08b\r",
                 b"echo ax\b \bb\r\nab\r\n")
        exchange("CRLF", b"echo one\r\n", b"echo one\r\none\r\n")
        exchange("after CRLF", b"echo two\n", b"echo two\r\ntwo\r\n")
        exchange("arrow sequence ignored", b"echo a\x1b[Db\r", b"echo ab\r\nab\r\n")
        boundary = b"echo " + b"x" * 122
        exchange("127 character boundary", boundary + b"\r",
                 boundary + b"\r\n" + b"x" * 122 + b"\r\n")
        exchange("overflow discards whole command", boundary + b"yz\r",
                 boundary + b"\a\r\nInput too long (max 127 characters); line discarded.\r\n")
        exchange("recovery after overflow", b"echo ok\r", b"echo ok\r\nok\r\n")
        print(f"PASS {count} checks; UART transcript: build/smoke.log")
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
        proc.stdin.close()
        proc.stdout.close()
        Path("build/smoke.log").write_bytes(transcript)


if __name__ == "__main__":
    main()
