import os
import struct
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
TEST_SH = ROOT / "test.sh"


def run_init(path: Path, input_text: str = "", snapshot: bool = True) -> str:
    env = os.environ.copy()
    if not snapshot:
        env["SLOPOS_NO_SNAPSHOT"] = "1"
    proc = subprocess.run(
        [str(TEST_SH), str(path)],
        cwd=str(ROOT),
        input=input_text.encode(),
        stdout=subprocess.PIPE,
        stderr=None,
        check=True,
        timeout=15,
        env=env,
    )
    out = proc.stdout.decode(errors="replace")
    out = out.replace("\r\n", "\n")
    if "SlopOS booting..." in out:
        out = out[out.index("SlopOS booting...") :]
    return out


def test_closure_script_runs():
    out = run_init(ROOT / "init_scripts" / "closure.scm")
    assert "SlopOS booting..." in out
    assert "closure test" in out
    assert "\n5\n" in out


def test_echo_read_string():
    out = run_init(ROOT / "init_scripts" / "echo.scm", "Slopcoder 2000\n")
    assert "SlopOS booting..." in out
    assert "Slopcoder 2000" in out


def test_alt_script_runs():
    out = run_init(ROOT / "init_scripts" / "alt.scm")
    assert "SlopOS booting..." in out
    assert "alt init running" in out


def test_gc_stress_script_runs():
    out = run_init(ROOT / "init_scripts" / "gc_stress.scm")
    assert "SlopOS booting..." in out
    assert "gc stress ok" in out


def test_spawn_threads_script_runs():
    out = run_init(ROOT / "init_scripts" / "spawn.scm")
    assert "SlopOS booting..." in out
    assert "thread1" in out
    assert "thread2" in out
    assert "t1done" in out
    assert "t2done" in out


def _read_file_from_fs(img_path: Path, filename: str) -> str:
    data = img_path.read_bytes()
    boot_len, fs_offset = struct.unpack_from("<II", data, 0)
    magic, version, dir_off, dir_len, data_off = struct.unpack_from("<8sIIII", data, fs_offset)
    entry_size = 64 + 4 + 4 + 4
    for i in range(0, dir_len, entry_size):
        off = fs_offset + dir_off + i
        name = data[off : off + 64].split(b"\0", 1)[0].decode("ascii")
        if name == filename:
            foff, flen, _ = struct.unpack_from("<III", data, off + 64)
            file_data = data[fs_offset + foff : fs_offset + foff + flen]
            return file_data.decode("utf-8", errors="replace")
    raise AssertionError(f"missing file {filename} in {img_path}")


def test_create_file_fixed_persists():
    out = run_init(ROOT / "init_scripts" / "write_fixed.scm", snapshot=False)
    assert "SlopOS booting..." in out
    fs_img = ROOT / "build" / "test_write_fixed_fs.img"
    contents = _read_file_from_fs(fs_img, "fixed.txt")
    assert contents == "fixed content"


def test_create_file_persists():
    out = run_init(ROOT / "init_scripts" / "write_in.scm", "Slopcoder 2000\n", snapshot=False)
    assert "SlopOS booting..." in out
    fs_img = ROOT / "build" / "test_write_in_fs.img"
    contents = _read_file_from_fs(fs_img, "input.txt")
    assert contents == "Slopcoder 2000"


def test_list_files():
    out = run_init(ROOT / "init_scripts" / "list_files.scm")
    assert "SlopOS booting..." in out
    assert "fs.scm" in out
    assert "init.scm" in out
