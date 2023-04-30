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
            ("pdr-cegar", "heuristic=pdr-pdb(pattern=cegar_pattern())"),
            ("pdr-rand", "heuristic=pdr-pdb(pattern=random_pattern(1000))"),
        ],
        start=1,
    )
]

BUILD_OPTIONS = []
DRIVER_OPTIONS = ["--overall-time-limit", "45m", "--overall-memory-limit", "6G"]

# TODO: change these
REVS = [
    ("PDR-thesis", "latest"),
    # ("a50720128335f5416b6da6f182a79d91ae63b895", "unoptimized"),
]

MATPLOTLIB_OPTIONS = {
    "figure.figsize": [2, 2],
    "savefig.dpi": 400,
}

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

# Attributes for algorithm comparisons
attributes = [
    "total_time",
#    "obligation_insertions",
#    "obligation_expansions",
#    "path_construction_time",
]
pairs_latest = [
    ("latest:01-pdr-noop", f"latest:{alg[0]}") for alg in CONFIGS[1:]
]
pairs = [
    # ("latest:01-pdr-noop", "unoptimized:01-pdr-noop"),
    # ("latest:06-pdr-greedy-1000", "unoptimized:06-pdr-greedy-1000"),
]

pairs += pairs_latest


def algo_format(alg_name):
    return alg_name.split(":")[1][7:]


suffix = "-rel" if project.RELATIVE else ""
for algo1, algo2 in pairs:
    for attr in attributes:
        # latest:01-pdr-noop
        algo1_name = algo_format(algo1)
        algo2_name = algo_format(algo2)
        exp.add_report(
            project.ScatterPlotReport(
                relative=project.RELATIVE,
                get_category=None if project.TEX else lambda run1, run2: run1["domain"],
                attributes=[attr],
                filter_algorithm=[algo1, algo2],
                filter=[project.add_evaluations_per_time],
                format="tex" if project.TEX else "png",
                matplotlib_options=MATPLOTLIB_OPTIONS
            ),
            name=f"{algo1_name}-vs-{algo2_name}-{attr}{suffix}",
        )


def add_generic_scatter(category_x, category_y, scale="both", get_category=None):
    if get_category is not None:
        categories = [(get_category,"")]
    else:
        categories = [
            (lambda run1: run1["domain"], "-domain"),
            (lambda run1: run1["algorithm"], "-alg"),
        ]
    if scale != "log" and scale != "linear" and scale != "both":
        raise ValueError(f"scale must be 'log', 'linear' or 'both' got: '{scale}'")
    for cat in categories:
        if scale == "linear" or scale == "both":
            exp.add_report(
                reports.ScatterPlotReport(
                    category_x,
                    category_y,
                    format="tex" if project.TEX else "png",
                    show_missing=False,
                    scale="linear",
                    get_category=cat[0],
                    matplotlib_options=MATPLOTLIB_OPTIONS
                ),
                name=f"{category_x}-vs-{category_y}{cat[1]}-linear"
            )
        if scale == "log" or scale == "both":
            exp.add_report(
                reports.ScatterPlotReport(
                    category_x,
                    category_y,
                    format="tex" if project.TEX else "png",
                    show_missing=False,
                    get_category=cat[0],
                    matplotlib_options=MATPLOTLIB_OPTIONS
                ),
                name=f"{category_x}-vs-{category_y}{cat[1]}"
            )


# TODO: make linear plot where it makes sense
add_generic_scatter("layer_size", "total_time")
add_generic_scatter("layer_size_literals", "clause_propagation_time")
add_generic_scatter("layer_size_literals", "extend_time")
add_generic_scatter("layer_size_literals", "path_construction_time")
add_generic_scatter("layer_size_literals", "total_time")
add_generic_scatter("layer_size_seeded", "obligation_insertions")
add_generic_scatter("layer_size_seeded", "total_time")
add_generic_scatter("obligation_expansions", "total_time")
add_generic_scatter("pattern_size", "layer_seed_time", "linear")
add_generic_scatter("pattern_size", "layer_size_seeded")
add_generic_scatter("pattern_size", "obligation_expansions")
add_generic_scatter("pattern_size", "obligation_insertions")
add_generic_scatter("pattern_size", "total_time")

exp.add_report(reports.LatexTable(
        x_attrs=["error", "total_time"],
        x_aggrs=[
            lambda prev, cur: prev if cur != "search-out-of-time" else prev + 1,
            lambda prev, cur: prev + 1,
        ],
        x_initial=[0, 0],
        show_header=False,
        y_formatter=algo_format
    ),
    name="tbl_out-of-time")


exp.run_steps()