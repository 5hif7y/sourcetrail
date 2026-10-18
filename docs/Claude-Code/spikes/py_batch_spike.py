"""Compare one Python process indexing many files against one process per file.

Usage: python py_batch_spike.py <indexer-dir> <source-dir>
"""
import glob
import os
import sqlite3
import subprocess
import sys
import tempfile
import time

indexer_dir = os.path.abspath(sys.argv[1])
source_dir = os.path.abspath(sys.argv[2])
run_py = os.path.join(indexer_dir, "run.py")

sys.path.insert(0, indexer_dir)
import indexer  # noqa: E402
import sourcetraildb as srctrl  # noqa: E402

files = sorted(glob.glob(os.path.join(source_dir, "*.py")))
if not files:
    raise SystemExit("no .py files in " + source_dir)

print("indexer:", indexer_dir)
print("files:", len(files))


def counts(db_path):
    db = sqlite3.connect(db_path)
    return (
        db.execute("select count(*) from node").fetchone()[0],
        db.execute("select count(*) from edge").fetchone()[0],
        db.execute("select count(*) from file").fetchone()[0],
    )


def one_process(db_path):
    if os.path.exists(db_path):
        os.remove(db_path)
    started = time.time()
    if not srctrl.open(db_path):
        raise SystemExit("open failed: " + srctrl.getLastError())
    client = indexer.AstVisitorClient()
    for path in files:
        srctrl.beginTransaction()
        indexer.indexSourceFile(path, None, source_dir, client, False)
        srctrl.commitTransaction()
    srctrl.close()
    return time.time() - started


def per_process(db_path):
    if os.path.exists(db_path):
        os.remove(db_path)
    started = time.time()
    for path in files:
        subprocess.run(
            [
                sys.executable,
                "-X",
                "utf8",
                run_py,
                "index",
                "--source-file-path",
                path,
                "--database-file-path",
                db_path,
            ],
            cwd=source_dir,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=True,
        )
    return time.time() - started


# Warm the OS cache and Python imports.
one_process(tempfile.mktemp(suffix=".srctrldb"))
per_process(tempfile.mktemp(suffix=".srctrldb"))

db_batch = tempfile.mktemp(suffix=".srctrldb")
db_per = tempfile.mktemp(suffix=".srctrldb")
t_batch = one_process(db_batch)
t_per = per_process(db_per)

print(f"one process:      {t_batch:6.2f} s  ({t_batch / len(files) * 1000:.0f} ms/file)  counts={counts(db_batch)}")
print(f"process per file: {t_per:6.2f} s  ({t_per / len(files) * 1000:.0f} ms/file)  counts={counts(db_per)}")
print(f"speedup: {t_per / t_batch:.2f}x  (saved {t_per - t_batch:.2f} s)")
