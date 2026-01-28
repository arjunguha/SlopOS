import os
import pty
import re
import select
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TEST_SH = ROOT / "test.sh"


@dataclass(frozen=True)
class Send:
    text: str
    slow: bool = True
    delay: float = 0.01


@dataclass(frozen=True)
class Sleep:
    seconds: float


@dataclass(frozen=True)
class Expect:
    text: str
    timeout: float = 5.0


def build_test_image(init_path: Path) -> tuple[Path, Path]:
    env = os.environ.copy()
    env["SLOPOS_BUILD_ONLY"] = "1"
    out = subprocess.check_output(
        [str(TEST_SH), str(init_path)],
        cwd=str(ROOT),
        env=env,
        stderr=None,
    ).decode(errors="replace")
    img = None
    fs_img = None
    for line in out.splitlines():
        if line.startswith("IMG="):
            img = line.split("=", 1)[1].strip()
        if line.startswith("FS_IMG="):
            fs_img = line.split("=", 1)[1].strip()
    if not img or not fs_img:
        raise RuntimeError(f"build-only output missing image paths:\n{out}")
    return Path(img), Path(fs_img)


def run_shell(init_path: Path, steps: list[object], timeout: float = 15.0) -> str:
    img, fs_img = build_test_image(init_path)
    qemu = os.environ.get("QEMU", "qemu-system-i386")
    args = [
        qemu,
        "-drive", f"if=floppy,format=raw,file={img}",
        "-drive", f"if=ide,format=raw,file={fs_img}",
        "-device", "isa-debug-exit,iobase=0xf4,iosize=0x04",
        "-display", "none",
        "-serial", "stdio",
        "-monitor", "none",
    ]

    master_fd, slave_fd = pty.openpty()
    proc = subprocess.Popen(args, stdin=slave_fd, stdout=slave_fd, stderr=None)
    os.close(slave_fd)

    output = bytearray()
    deadline = time.time() + timeout

    def drain_output(max_wait: float = 0.05) -> None:
        end = time.time() + max_wait
        while True:
            remaining = max(0.0, end - time.time())
            if remaining == 0:
                break
            ready, _, _ = select.select([master_fd], [], [], remaining)
            if not ready:
                break
            try:
                chunk = os.read(master_fd, 4096)
            except OSError:
                break
            if not chunk:
                break
            output.extend(chunk)

    def read_until(text: str, wait: float) -> None:
        end = time.time() + wait
        while True:
            if text.encode() in output:
                return
            if time.time() > end:
                raise TimeoutError(f"timeout waiting for {text!r}")
            drain_output(0.1)

    def send_text(text: str, slow: bool, delay: float) -> None:
        data = text.encode()
        if slow:
            for b in data:
                os.write(master_fd, bytes([b]))
                time.sleep(delay)
        else:
            os.write(master_fd, data)

    try:
        for step in steps:
            if time.time() > deadline:
                raise TimeoutError("shell run timed out")
            if isinstance(step, Send):
                send_text(step.text, step.slow, step.delay)
                drain_output()
            elif isinstance(step, Sleep):
                time.sleep(step.seconds)
                drain_output()
            elif isinstance(step, Expect):
                read_until(step.text, step.timeout)
            else:
                raise ValueError(f"unknown step: {step!r}")

        while proc.poll() is None and time.time() < deadline:
            drain_output(0.1)
    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=0.5)
            except subprocess.TimeoutExpired:
                proc.kill()
        drain_output(0.1)
        os.close(master_fd)

    out = output.decode(errors="replace")
    return out.replace("\r\n", "\n")
