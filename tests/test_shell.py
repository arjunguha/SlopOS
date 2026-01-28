from pathlib import Path

from shell_runner import Expect, Send, run_shell

ROOT = Path(__file__).resolve().parent.parent
SHELL_INIT = ROOT / "init_scripts" / "shell.scm"


def prompt() -> Expect:
    return Expect("> ")


def run_shell_steps(steps, timeout=10):
    return run_shell(SHELL_INIT, steps, timeout=timeout)


def test_shell_infra_create_and_ls():
    steps = [
        prompt(),
        Send("create note.txt\n"),
        Send("hello shell\n"),
        Send("EOF\n"),
        Expect("ok"),
        prompt(),
        Send("ls\n"),
        Expect("note.txt"),
        Send("exit\n"),
    ]
    out = run_shell_steps(steps, timeout=10)
    assert "note.txt" in out


def test_shell_ls_lists_core_files():
    steps = [
        prompt(),
        Send("ls\n"),
        Expect("fs.scm"),
        Expect("init.scm"),
        Send("exit\n"),
    ]
    out = run_shell_steps(steps, timeout=10)
    assert "fs.scm" in out
    assert "init.scm" in out


def test_shell_cat_missing_file():
    steps = [
        prompt(),
        Send("cat missing.txt\n"),
        Expect("missing file"),
        Send("exit\n"),
    ]
    out = run_shell_steps(steps, timeout=10)
    assert "missing file" in out


def test_shell_create_and_cat_file():
    steps = [
        prompt(),
        Send("create hello.txt\n"),
        Send("hi there\n"),
        Send("EOF\n"),
        Expect("ok"),
        prompt(),
        Send("cat hello.txt\n"),
        Expect("hi there"),
        Send("exit\n"),
    ]
    out = run_shell_steps(steps, timeout=10)
    assert "hi there" in out


def test_shell_create_two_files_and_ls():
    steps = [
        prompt(),
        Send("create foo.txt\n"),
        Send("foo\n"),
        Send("EOF\n"),
        Expect("ok"),
        prompt(),
        Send("create bar.txt\n"),
        Send("bar\n"),
        Send("EOF\n"),
        Expect("ok"),
        prompt(),
        Send("ls\n"),
        Expect("foo.txt"),
        Expect("bar.txt"),
        Send("exit\n"),
    ]
    out = run_shell_steps(steps, timeout=10)
    assert "foo.txt" in out
    assert "bar.txt" in out


def test_shell_exec_program():
    steps = [
        prompt(),
        Send("create prog.scm\n"),
        Send("(display \"exec ok\")\n"),
        Send("(newline)\n"),
        Send("EOF\n"),
        Expect("ok"),
        prompt(),
        Send("exec prog.scm\n"),
        Expect("exec ok"),
        Send("exit\n"),
    ]
    out = run_shell_steps(steps, timeout=10)
    assert "exec ok" in out


def test_shell_exec_missing_program():
    steps = [
        prompt(),
        Send("exec nope.scm\n"),
        Expect("missing file"),
        Send("exit\n"),
    ]
    out = run_shell_steps(steps, timeout=10)
    assert "missing file" in out


def test_shell_create_multiline_cat():
    steps = [
        prompt(),
        Send("create notes.txt\n"),
        Send("alpha\n"),
        Send("beta\n"),
        Send("EOF\n"),
        Expect("ok"),
        prompt(),
        Send("cat notes.txt\n"),
        Expect("alpha"),
        Expect("beta"),
        Send("exit\n"),
    ]
    out = run_shell_steps(steps, timeout=10)
    assert "alpha" in out
    assert "beta" in out


def test_shell_create_empty_file():
    steps = [
        prompt(),
        Send("create empty.txt\n"),
        Send("EOF\n"),
        Expect("ok"),
        prompt(),
        Send("ls\n"),
        Expect("empty.txt"),
        Send("exit\n"),
    ]
    out = run_shell_steps(steps, timeout=10)
    assert "empty.txt" in out


def test_shell_exec_reads_other_file():
    steps = [
        prompt(),
        Send("create data.txt\n"),
        Send("payload\n"),
        Send("EOF\n"),
        Expect("ok"),
        prompt(),
        Send("create show.scm\n"),
        Send("(display (read-text-file \"data.txt\"))\n"),
        Send("(newline)\n"),
        Send("EOF\n"),
        Expect("ok"),
        prompt(),
        Send("exec show.scm\n"),
        Expect("payload"),
        Send("exit\n"),
    ]
    out = run_shell_steps(steps, timeout=10)
    assert "payload" in out


def test_shell_unknown_command():
    steps = [
        prompt(),
        Send("wat\n"),
        Expect("unknown command"),
        Send("exit\n"),
    ]
    out = run_shell_steps(steps, timeout=10)
    assert "unknown command" in out


def test_shell_define_and_display():
    steps = [
        prompt(),
        Send("create calc.scm\n"),
        Send("(define x 7)\n"),
        Send("(display (+ x 3))\n"),
        Send("(newline)\n"),
        Send("EOF\n"),
        Expect("ok"),
        prompt(),
        Send("exec calc.scm\n"),
        Expect("10"),
        Send("exit\n"),
    ]
    out = run_shell_steps(steps, timeout=10)
    assert "10" in out


def test_shell_create_file_with_spaces():
    steps = [
        prompt(),
        Send("create story.txt\n"),
        Send("hello world\n"),
        Send("EOF\n"),
        Expect("ok"),
        prompt(),
        Send("cat story.txt\n"),
        Expect("hello world"),
        Send("exit\n"),
    ]
    out = run_shell_steps(steps, timeout=10)
    assert "hello world" in out
