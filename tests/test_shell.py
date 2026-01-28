from pathlib import Path

from shell_runner import Expect, Send, run_shell

ROOT = Path(__file__).resolve().parent.parent


def test_shell_infra_create_and_ls():
    steps = [
        Expect("> "),
        Send("create note.txt\n"),
        Send("hello shell\n"),
        Send("EOF\n"),
        Expect("ok"),
        Expect("> "),
        Send("ls\n"),
        Expect("note.txt"),
        Send("exit\n"),
    ]
    out = run_shell(ROOT / "init_scripts" / "shell.scm", steps, timeout=10)
    assert "note.txt" in out
