#!/usr/bin/env python3
"""Assertions over Sourcetrail databases produced by the native Odin indexer."""

import argparse
import sqlite3
import sys


def display_name(serialized: str) -> str:
    """Turn a serialized name hierarchy into `package.Symbol.field` form."""
    names = []
    for part in serialized.split("\t"):
        if not part or part in ("s", "p", ".", "/"):
            continue
        if len(part) > 1 and part[0] in ("m", "n", "f"):
            part = part[1:]
        names.append(part)
    return ".".join(names)


def read_database(path: str):
    connection = sqlite3.connect(path)
    nodes = {
        display_name(row[0])
        for row in connection.execute("select serialized_name from node")
    }
    edges = {
        (display_name(source), edge_type, display_name(target))
        for source, edge_type, target in connection.execute(
            """
            select source.serialized_name, edge.type, target.serialized_name
            from edge
            join node source on source.id = edge.source_node_id
            join node target on target.id = edge.target_node_id
            """
        )
    }
    connection.close()
    return nodes, edges


def split_names(value: str):
    return [item.strip() for item in value.split(",") if item.strip()]


def parse_edge(value: str):
    source, edge_type, target = value.split("|")
    return source, int(edge_type), target


def report_missing(label, expected, actual):
    missing = sorted(expected - actual)
    if missing:
        print(f"{label} missing: {missing}")
    return bool(missing)


def command_compare(args) -> int:
    nodes_a, edges_a = read_database(args.a)
    nodes_b, edges_b = read_database(args.b)

    print(f"A: {len(nodes_a)} nodes, {len(edges_a)} edges")
    print(f"B: {len(nodes_b)} nodes, {len(edges_b)} edges")

    failed = report_missing("nodes only in A", nodes_a, nodes_b)
    failed |= report_missing("nodes only in B", nodes_b, nodes_a)
    failed |= report_missing("edges only in A", edges_a, edges_b)
    failed |= report_missing("edges only in B", edges_b, edges_a)
    return 1 if failed else 0


def command_check(args) -> int:
    nodes, edges = read_database(args.db)
    failed = False

    required = set(split_names(args.require)) if args.require else set()
    forbidden = set(split_names(args.forbid)) if args.forbid else set()
    required_edges = {parse_edge(value) for value in args.require_edge or []}
    forbidden_edges = {parse_edge(value) for value in args.forbid_edge or []}

    failed |= report_missing("required nodes", required, nodes)
    present_forbidden = sorted(forbidden & nodes)
    if present_forbidden:
        print(f"forbidden nodes present: {present_forbidden}")
        failed = True
    failed |= report_missing("required edges", required_edges, edges)
    present_forbidden_edges = sorted(forbidden_edges & edges)
    if present_forbidden_edges:
        print(f"forbidden edges present: {present_forbidden_edges}")
        failed = True

    if args.unsolved is not None:
        unsolved = sum(1 for node in nodes if node == "unsolved symbol")
        if unsolved != args.unsolved:
            print(f"unsolved symbol nodes: {unsolved} (expected {args.unsolved})")
            failed = True

    if args.min_edges is not None and len(edges) < args.min_edges:
        print(f"edges: {len(edges)} (expected at least {args.min_edges})")
        failed = True

    if not failed:
        print(f"ok: {len(nodes)} nodes, {len(edges)} edges")
    return 1 if failed else 0


def main() -> int:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    compare = subparsers.add_parser("compare")
    compare.add_argument("a")
    compare.add_argument("b")
    compare.set_defaults(func=command_compare)

    check = subparsers.add_parser("check")
    check.add_argument("db")
    check.add_argument("--require", default="")
    check.add_argument("--forbid", default="")
    check.add_argument("--require-edge", action="append")
    check.add_argument("--forbid-edge", action="append")
    check.add_argument("--unsolved", type=int, default=None)
    check.add_argument("--min-edges", type=int, default=None)
    check.set_defaults(func=command_check)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
