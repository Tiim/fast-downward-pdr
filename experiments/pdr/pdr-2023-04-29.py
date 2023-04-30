#! /usr/bin/env python

import os

import project
import reports


REPO = project.get_repo_base()
BENCHMARKS_DIR = os.environ["DOWNWARD_BENCHMARKS"]

# If REVISION_CACHE is None, the default ./data/revision-cache is used.
REVISION_CACHE = os.environ.get("DOWNWARD_REVISION_CACHE")

if project.REMOTE:
    SUITE = project.SUITE_UNIT_COST
    ENV = project.BaselSlurmEnvironment(email="tim.bachmann@stud.unibas.ch", partition="infai_2")
else:
    SUITE = ["blocks:probBLOCKS-8-0.pddl"]  # project.SUITE_UNIT_COST[:1]
    ENV = project.LocalEnvironment(processes=4)

# TODO: change these to pdr
CONFIGS = [
    (f"{index:02d}-{h_nick}", ["--search", f"pdr({h})"])
    for index, (h_nick, h) in enumerate(
        [
            ("pdr-noop", "heuristic=pdr-noop()"),
            ("pdr-greedy-50", "heuristic=pdr-pdb(pattern=greedy(50))"),
            ("pdr-greedy-100", "heuristic=pdr-pdb(pattern=greedy(100))"),
            ("pdr-greedy-200", "heuristic=pdr-pdb(pattern=greedy(200))"),
            ("pdr-greedy-500", "heuristic=pdr-pdb(pattern=greedy(500))"),
            ("pdr-greedy-1000", "heuristic=pdr-pdb(pattern=greedy(1000))"),
            ("pdr-greedy-5000", "heuristic=pdr-pdb(pattern=greedy(5000))"),
            ("pdr-cegar", "heuristic=pdr-pdb(pattern=cegar_pattern())"),
            ("pdr-rand", "heuristic=pdr-pdb(pattern=random_pattern(1000))"),
            ("pdr-rand-5000", "heuristic=pdr-pdb(pattern=random_pattern(5000))"),
        ],
        start=1,
    )
]

BUILD_OPTIONS = []
DRIVER_OPTIONS = ["--overall-time-limit", "120m", "--overall-memory-limit", "6G"]

# TODO: change these
REVS = [
    ("PDR-thesis", "latest"),
    # ("a50720128335f5416b6da6f182a79d91ae63b895", "unoptimized"),
]


ATTRIBUTES = [
    "error",
    "run_dir",
    "total_time",
    "coverage",
    "memory",
    "layer_seed_time",
    "layer_size",
    "layer_size_seeded",
    "obligation_expansions",
    "obligation_insertions",
    "pdr_projected_states",
]

exp = project.CustomFastDownwardExperiment(
    environment=ENV, revision_cache=REVISION_CACHE)
for config_nick, config in CONFIGS:
    for rev, rev_nick in REVS:
        algo_name = f"{rev_nick}:{config_nick}" if rev_nick else config_nick
        exp.add_algorithm(
            algo_name,
            REPO,
            rev,
            config,
            build_options=BUILD_OPTIONS,
            driver_options=DRIVER_OPTIONS,
        )
exp.add_suite(BENCHMARKS_DIR, SUITE)

exp.add_parser(exp.EXITCODE_PARSER)
exp.add_parser(exp.TRANSLATOR_PARSER)
exp.add_parser(exp.SINGLE_SEARCH_PARSER)
exp.add_parser(project.DIR / "parser.py")
exp.add_parser(exp.PLANNER_PARSER)

exp.add_step("build", exp.build)
exp.add_step("start", exp.start_runs)
exp.add_parse_again_step()
exp.add_fetcher(name="fetch")
exp.init_reports()

project.add_absolute_report(
    exp, attributes=ATTRIBUTES, filter=[project.add_evaluations_per_time]
)
project.add_publish_step(exp)
project.add_download_step(exp)


def filter_zero(prop):
    def f(run):
        if prop in run and run[prop] <= 0:
            run[prop] = None
        return True
    return f


# Attributes for algorithm comparisons
attributes = [
    ("total_time", []),
    ("obligation_insertions", filter_zero("obligation_insertions")),
    ("obligation_expansions", filter_zero("obligation_expansions")),
    # "path_construction_time",
    ("pattern_size", []),
    ("layer_size_literals", []),
    ("memory", []),
]
pairs = [
    ("latest:01-pdr-noop", f"latest:{alg[0]}") for alg in CONFIGS[1:]
]


project.add_reports(exp=exp, pairs=pairs, attributes=attributes, CONFIGS=CONFIGS)

exp.run_steps()
