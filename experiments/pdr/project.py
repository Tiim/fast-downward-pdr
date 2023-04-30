from pathlib import Path
import reports
import platform
import re
import subprocess
import sys

from downward.experiment import FastDownwardExperiment
from downward.reports.absolute import AbsoluteReport
from downward.reports.scatter import ScatterPlotReport
from downward.reports.taskwise import TaskwiseReport
from lab import tools, environments
from lab.environments import (
    BaselSlurmEnvironment,
    LocalEnvironment,
    TetralithEnvironment,
)
from lab.experiment import ARGPARSER
from lab.reports import Attribute, geometric_mean
from lab.steps import get_step

# Silence import-unused messages. Experiment scripts may use these imports.
assert (
    BaselSlurmEnvironment
    and FastDownwardExperiment
    and LocalEnvironment
    and ScatterPlotReport
    and TaskwiseReport
    and TetralithEnvironment
)


DIR = Path(__file__).resolve().parent
NODE = platform.node()
# Cover both the Basel and Linköping clusters for simplicity.
REMOTE = NODE.endswith((".scicore.unibas.ch", ".cluster.bc2.ch")) or re.match(
    r"tetralith\d+\.nsc\.liu\.se|n\d+", NODE
)


def parse_args():
    ARGPARSER.add_argument("--tex", action="store_true",
                           help="produce LaTeX output")
    ARGPARSER.add_argument(
        "--relative", action="store_true", help="make relative scatter plots"
    )
    return ARGPARSER.parse_args()


ARGS = parse_args()
TEX = ARGS.tex
RELATIVE = True  # ARGS.relative

EVALUATIONS_PER_TIME = Attribute(
    "evaluations_per_time", min_wins=False, function=geometric_mean, digits=1
)
MATPLOTLIB_OPTIONS = {
    "figure.figsize": [3, 3],
    "savefig.dpi": 200,
} if TEX else None

# fmt: off

SUITE_UNIT_COST = ["airport", "barman-opt14-strips", "blocks", "childsnack-opt14-strips", "depot", "driverlog", "freecell", "grid", "gripper", "hiking-opt14-strips", "logistics00", "logistics98", "miconic", "movie", "mprime", "mystery", "nomystery-opt11-strips", "openstacks-strips", "organic-synthesis-opt18-strips", "parking-opt11-strips", "parking-opt14-strips", "pathways", "pipesworld-notankage", "pipesworld-tankage", "psr-small", "rovers", "satellite", "snake-opt18-strips", "storage", "termes-opt18-strips", "tidybot-opt11-strips", "tidybot-opt14-strips", "tpp", "trucks-strips", "visitall-opt11-strips", "visitall-opt14-strips", "zenotravel"]

# fmt: on


def get_repo_base() -> Path:
    """Get base directory of the repository, as an absolute path.

    Search upwards in the directory tree from the main script until a
    directory with a subdirectory named ".git" is found.

    Abort if the repo base cannot be found."""
    path = Path(tools.get_script_path())
    while path.parent != path:
        if (path / ".git").is_dir():
            return path
        path = path.parent
    sys.exit("repo base could not be found")


def remove_file(path: Path):
    try:
        path.unlink()
    except FileNotFoundError:
        pass


def add_evaluations_per_time(run):
    evaluations = run.get("evaluations")
    time = run.get("search_time")
    if evaluations is not None and evaluations >= 100 and time:
        run["evaluations_per_time"] = evaluations / time
    return run


def _get_exp_dir_relative_to_repo():
    repo_name = get_repo_base().name
    script = Path(tools.get_script_path())
    script_dir = script.parent
    rel_script_dir = script_dir.relative_to(get_repo_base())
    expname = script.stem
    return repo_name / rel_script_dir / "data" / expname


def _get_eval_dir():
    script = Path(tools.get_script_path())
    expname = script.stem + "-eval"
    return Path("data") / expname


def add_absolute_report(exp, *, name=None, outfile=None, **kwargs):
    report = AbsoluteReport(**kwargs)
    if name and not outfile:
        outfile = f"{name}.{report.output_format}"
    elif outfile and not name:
        name = Path(outfile).name
    elif not name and not outfile:
        name = f"{exp.name}-abs"
        outfile = f"{name}.{report.output_format}"

    if not Path(outfile).is_absolute():
        outfile = Path(exp.eval_dir) / outfile

    exp.add_report(report, name=name, outfile=outfile)
    if not REMOTE:
        exp.add_step(f"open-{name}", subprocess.call, ["xdg-open", outfile])
    # exp.add_step(f"publish-{name}", subprocess.call, ["publish", outfile])


def add_publish_step(exp):
    exp.add_step("publish", publish_properties)


def add_download_step(exp):
    exp.add_step("download", read_properties)


def publish_properties():
    p_file = _get_eval_dir() / "properties"
    urlfile = _get_eval_dir() / 'url.txt'
    import requests

    with open(p_file, 'rb') as f:
        response = requests.post('https://0x0.st', files={'file': f})
        print(response.text)
        with open(urlfile, 'w') as url_file:
            url_file.write(response.text)


def read_properties():
    import requests
    import os

    p_file = _get_eval_dir() / "properties"
    os.makedirs(_get_eval_dir(), exist_ok=True)

    url = input("Enter URL: ")
    response = requests.get(url)
    with open(p_file, 'wb') as f:
        f.write(response.content)


class CustomFastDownwardExperiment (FastDownwardExperiment):
    def __init__(self, path=None, environment=None, revision_cache=None):
        FastDownwardExperiment.__init__(
            self, path=path, environment=environment, revision_cache=revision_cache)
        self.report_steps = []

    def add_report(self, report, name="", eval_dir="", outfile=""):
        FastDownwardExperiment.add_report(
            self, report, name=name, eval_dir=eval_dir, outfile=outfile)
        last_step = self.steps.pop()
        self.report_steps.append(last_step)

    def init_reports(self):
        self.add_step("create_reports", self.run_reports)

    def run_reports(self):
        env = environments.LocalEnvironment()
        env.run_steps(self.report_steps)


def add_generic_scatter(exp, category_x, category_y, scale="both", get_category=None):
    if get_category is not None:
        categories = [(get_category, "")]
    else:
        categories = [
            (lambda run1: run1["domain"], "-domain"),
            (lambda run1: run1["algorithm"], "-alg"),
        ]
    if scale != "log" and scale != "linear" and scale != "both":
        raise ValueError(
            f"scale must be 'log', 'linear' or 'both' got: '{scale}'")
    for cat in categories:
        if scale == "linear" or scale == "both":
            exp.add_report(
                reports.ScatterPlotReport(
                    category_x,
                    category_y,
                    format="tex" if TEX else "png",
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
                    format="tex" if TEX else "png",
                    show_missing=False,
                    get_category=cat[0],
                    matplotlib_options=MATPLOTLIB_OPTIONS
                ),
                name=f"{category_x}-vs-{category_y}{cat[1]}"
            )


def algo_format(alg_name):
    return alg_name.split(":")[1][7:]


def add_reports(exp, pairs, attributes, CONFIGS):

    exp.add_report(reports.LatexTable(
        x_attrs=[
            "error",
            # "total_time"
        ],
        x_aggrs=[
            lambda prev, cur: prev if cur != "search-out-of-time" else prev + 1,
            # lambda prev, cur: prev + 1,
        ],
        # x_initial=[0, 0],
        show_header=False,
        y_formatter=algo_format
    ),
        name="tbl_out-of-time")
    exp.add_report(reports.TikZBarChart(
        x_attrs=[
            "layer_size_seeded_total",
        ],
        y_formatter=algo_format,
        y_label="Average Seeded Clauses"
    ),
        name="bar_chart-layer_size_seeded_total")

    suffix = "-rel" if RELATIVE else ""
    for algo1, algo2 in pairs:
        for attr, filters, category in attributes:
            # latest:01-pdr-noop
            algo1_name = algo_format(algo1)
            algo2_name = algo_format(algo2)
            exp.add_report(
                ScatterPlotReport(
                    relative=RELATIVE,
                    # None if TEX else lambda run1, run2: run1["domain"],
                    get_category=category,
                    attributes=[attr],
                    filter_algorithm=[algo1, algo2],
                    filter=filters,
                    format="tex" if TEX else "png",
                    show_missing=False,
                    matplotlib_options=MATPLOTLIB_OPTIONS
                ),
                name=f"{algo1_name}-vs-{algo2_name}-{attr}{suffix}",
            )

    add_generic_scatter(exp, "layer_size", "total_time", scale="log")
    # add_generic_scatter(exp, "layer_size_literals", "clause_propagation_time")
    # add_generic_scatter(exp, "layer_size_literals", "extend_time")
    # add_generic_scatter(exp, "layer_size_literals", "path_construction_time")
    add_generic_scatter(exp, "layer_size_literals", "total_time", scale="log")
    # add_generic_scatter(exp, "layer_size_seeded", "obligation_insertions")
    # add_generic_scatter(exp, "layer_size_seeded", "total_time")
    # add_generic_scatter(exp, "obligation_expansions", "total_time")
    # add_generic_scatter(exp, "pattern_size", "layer_seed_time", "linear")
    # add_generic_scatter(exp, "pattern_size", "layer_size_seeded")
    # add_generic_scatter(exp, "pattern_size", "obligation_expansions")
    # add_generic_scatter(exp, "pattern_size", "obligation_insertions")
    # add_generic_scatter(exp, "pattern_size", "total_time")

    # add_generic_scatter(exp, "pdr_projected_states", "total_time")
    # add_generic_scatter(exp, "pdr_projected_states", "obligation_expansions")
    # add_generic_scatter(exp, "pdr_projected_states", "obligation_insertions")
    # add_generic_scatter(exp, "pdr_projected_states", "layer_size_literals")
