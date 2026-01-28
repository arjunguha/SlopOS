import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
TEST_SH = ROOT / "test.sh"


def run_init(path: Path, input_text: str = "") -> str:
    proc = subprocess.run(
        [str(TEST_SH), str(path)],
        cwd=str(ROOT),
        input=input_text.encode(),
        stdout=subprocess.PIPE,
        stderr=None,
        check=True,
        timeout=15,
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
