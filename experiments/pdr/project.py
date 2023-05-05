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

REPLACE = [("legend pos={outer north east}", "legend pos={south west}")]

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


def add_generic_scatter(exp, category_x, category_y, scale="both", get_category=None, filter=[]):
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
                reports.ReportWrapperReplacer(reports.ScatterPlotReport(
                    category_x,
                    category_y,
                    format="tex" if TEX else "png",
                    show_missing=False,
                    filter=filter,
                    scale="linear",
                    get_category=cat[0],
                    matplotlib_options=MATPLOTLIB_OPTIONS
                ),
                    replace=REPLACE),
                name=f"{category_x}-vs-{category_y}{cat[1]}-linear"
            )
        if scale == "log" or scale == "both":
            exp.add_report(
                reports.ReportWrapperReplacer(reports.ScatterPlotReport(
                    category_x,
                    category_y,
                    format="tex" if TEX else "png",
                    show_missing=False,
                    filter=filter,
                    get_category=cat[0],
                    matplotlib_options=MATPLOTLIB_OPTIONS
                ),
                    replace=REPLACE),
                name=f"{category_x}-vs-{category_y}{cat[1]}"
            )


def algo_format(alg_name):
    return alg_name.split(":")[1][7:]


def filter_zero(prop):
    def f(run):
        if prop in run and run[prop] <= 0:
            run[prop] = None
        return True
    return f


def filter_exclude_algs(algs):
    if isinstance(algs, str):
        algs = [algs]

    def f(run):
        return not (algo_format(run["algorithm"]) in algs)
    return f


def add_first_layer_filter(run):
    run["layer_size_first"] = (
        run["layer_size"][0] if "layer_size" in run and len(
            run["layer_size"]) > 0 else 0
    )
    return True


def categorize_by_comparison(prop, less="less", more="more", missing="missing"):
    def category(run1, run2):
        if prop not in run1 or prop not in run2:
            return "\\vphantom{3}"+missing
        elif run1[prop] > run2[prop]:
            return "\\vphantom{1}"+less
        else:
            return "\\vphantom{2}"+more
    return category


def add_reports(exp, pairs,  CONFIGS):

    excluded_algs = ["greedy-200", "greedy-5000", "rand-5000"]
    exp.add_report(reports.LambdaTexReport(
        lambda data: max([(run["pdb_projected_states"]
                           if "pdb_projected_states" in run and "cegar" in run["algorithm"] else 0) for alg, run in data.items()]),
        filter=[filter_exclude_algs(excluded_algs)],
    ),
        name="cegar_max_projected_states")

    def less_clauses_faster(data):
        table = {}
        for id, run in data.items():
            domain = run["domain"]
            problem = run["problem"]
            alg = run["algorithm"]
            if alg not in table:
                table[alg] = {
                    "few_and_faster": 0,
                    "few": 0,
                    "more_and_slower": 0,
                    "more": 0,
                    "total": 0,
                }
            ref = data[f"latest:01-pdr-noop-{domain}-{problem}"]

            if "layer_size_first" not in ref or "layer_size_first" not in run or "total_time" not in ref or "total_time" not in run:
                continue

            table[alg]["total"] += 1
            if run["layer_size_first"] < ref["layer_size_first"]:
                table[alg]["few"] += 1
                if run["total_time"] < ref["total_time"]:
                    table[alg]["few_and_faster"] += 1
            if run["layer_size_first"] > ref["layer_size_first"]:
                table[alg]["more"] += 1
                if run["total_time"] > ref["total_time"]:
                    table[alg]["more_and_slower"] += 1

        lines = []
        for k, t in table.items():
            if t["few"] != 0:
                lines.append(
                    k + " " + str((t["few_and_faster"] / t["few"]) * 100) + "%")
            if t["more"] != 0:
                lines.append(
                    k + " " + str((t["more_and_slower"] / t["more"]) * 100) + "%")

        return "\n".join(lines)

    def avg_variance(data):
        table = {}
        for id, run in data.items():
            domain = run["domain"]
            problem = run["problem"]
            alg = run["algorithm"]
            if alg not in table:
                table[alg] = []
            ref = data[f"latest:01-pdr-noop-{domain}-{problem}"]

            if "total_time" not in ref or "total_time" not in run or run["error"] != "success" or ref["error"] != "success":
                continue
            runt = run["total_time"]
            reft = ref["total_time"]
            if runt == 0 or reft == 0:
                continue

            table[alg].append(runt/reft)

        lines = []
        for k, t in table.items():
            mean = sum(t) / len(t)
            var = sum((i - mean) ** 2 for i in t) / len(t)
            lines.append(f"{k} mean: {mean}, variance: {var}")

        return "\n".join(lines)

    exp.add_report(reports.LambdaTexReport(
        avg_variance,
        filter=[filter_exclude_algs(excluded_algs)],
    ),
        name="mean_variance")
    exp.add_report(reports.LambdaTexReport(
        less_clauses_faster,
        filter=[add_first_layer_filter, filter_exclude_algs(excluded_algs)],
    ),
        name="less_clauses_faster")

    def find_task_with_high_obligation_difference(data):
        probs = {}
        for _, run in data.items():
            alg = algo_format(run["algorithm"])
            prob = run["problem"]
            if prob not in probs:
                probs[prob] = {}
            probs[prob][alg] = run
        output = []
        for prob, algs in probs.items():
            print(algs)
            greedy = algs["greedy-1000"]
            noop = algs["noop"]
            greedy_ob = greedy["obligation_expansions"] if "obligation_expansions" in greedy else 0
            noop_ob = noop["obligation_expansions"] if "obligation_expansions" in noop else 0
            if noop_ob > greedy_ob or noop_ob == 0 or greedy_ob == 0:
                continue
            diff = greedy_ob - noop_ob
            time = max(greedy["total_time"], noop["total_time"])
            output.append(
                f"diff obligation_expansions: {diff:06d}, greedy exp: {greedy_ob}, noop exp: {noop_ob}, prob: {prob}, max_time: {time}")
        return "\n".join(output)
    exp.add_report(
        reports.LambdaTexReport(find_task_with_high_obligation_difference,
                                filter=[lambda run: algo_format(run["algorithm"]) in ["noop", "greedy-1000"]]),
        name="task_with_high_obligation_difference")

    exp.add_report(reports.LatexTable(
        x_attrs=[
            "error",
            "error",
            "error",
        ],
        x_aggrs=[
            lambda prev, cur: prev +
            1 if cur in ["success", "search-unsolvable-incomplete"] else prev,
            lambda prev, cur: prev + 1 if cur == "search-out-of-time" else prev,
            lambda prev, cur: prev + 1 if cur not in [
                "success", "search-out-of-time", "search-unsolvable-incomplete"] else prev,
            # lambda prev, cur: prev + 1,
        ],
        filter=[filter_exclude_algs(excluded_algs)],
        x_initial=[0, 0, 0],
        show_header=False,
        y_formatter=algo_format
    ),
        name="tbl_out-of-time")

    exp.add_report(reports.LatexCoverate(
        filter=[filter_exclude_algs(excluded_algs)]), name="coverage")

    exp.add_report(reports.LatexTable(
        x_attrs=[
            "error",
            "error",
            "error",
        ],
        x_aggrs=[
            lambda prev, cur: prev +
            1 if cur in ["success", "search-unsolvable-incomplete"] else prev,
            lambda prev, cur: prev + 1 if cur == "search-out-of-time" else prev,
            lambda prev, cur: prev + 1 if cur not in [
                "success", "search-out-of-time", "search-unsolvable-incomplete"] else prev,
            # lambda prev, cur: prev + 1,
        ],
        filter=[filter_exclude_algs(excluded_algs)],
        x_initial=[0, 0, 0],
        show_header=False,
        y_formatter=algo_format
    ),
        name="tbl_out-of-time")

    def add_l0(run):
        run["layer_size_seeded_l0"] = run["layer_size_seeded"][0] if len(
            run["layer_size_seeded"]) > 0 else 0
        return True
    exp.add_report(reports.TikZBarChart(
        x_attrs=[
            "layer_size_seeded_l0",
        ],
        filter=[filter_exclude_algs(
            ["noop"] + excluded_algs), add_l0],
        y_formatter=algo_format,
        y_label="Average Seeded Clauses"
    ),
        name="bar_chart-layer_size_seeded_total")

    attributes = [
        {
            "attribute": "total_time",
            "filter": [],
            "category": categorize_by_comparison(
                "obligation_expansions", more="more expansions",
                less="fewer expansions", missing="at least one run failed"),
            "show_missing": True,
            "relative": True,
        },        {
            "attribute": "total_time",
            "filter": [],
            "category": lambda run, run2: run["domain"],
            "show_missing": True,
            "relative": True,
            "suffix": "-domain"
        },
        {
            "attribute": "obligation_insertions",
            "filter": [
                filter_zero("obligation_insertions")
            ],
            "category": categorize_by_comparison(
                "total_time", more="slower", less="faster",
                missing="at least one run failed"),
            "show_missing": False,
            "relative": True,
        },
        {
            "attribute": "obligation_expansions",
            "filter": [
                filter_zero("obligation_expansions")
            ],
            "category": categorize_by_comparison(
                "total_time", more="slower", less="faster",
                missing="at least one run failed"),
            "show_missing": False,
            "relative": True,
        },
        {
            "attribute": "layer_size_first",
            "filter": [
                add_first_layer_filter,
                filter_zero("layer_size_first")
            ],
            "category": categorize_by_comparison(
                "total_time", more="slower", less="faster",
                missing="at least one run failed"),
            "show_missing": False,
            "relative": True,
        },
        {
            "attribute": "layer_size_first",
            "filter": [
                add_first_layer_filter,
                filter_zero("layer_size_first")
            ],
            "category": categorize_by_comparison(
                "obligation_expansions", more="more expansions",
                less="fewer expansions", missing="at least one run failed"),
            "show_missing": False,
            "relative": True,
            "suffix": "obs"
        },
        # "path_construction_time",
    ]
    for algo1, algo2 in pairs:
        algo1_name = algo_format(algo1)
        algo2_name = algo_format(algo2)

        if algo1_name in excluded_algs or algo2_name in excluded_algs:
            continue

        for a in attributes:
            # latest:01-pdr-noop
            attribute = a["attribute"]
            suffix = a["suffix"] if "suffix" in a else ""
            exp.add_report(
                reports.ReportWrapperReplacer(ScatterPlotReport(
                    relative=a["relative"],
                    # None if TEX else lambda run1, run2: run1["domain"],
                    get_category=a["category"],
                    attributes=[attribute],
                    filter_algorithm=[algo1, algo2],
                    filter=a["filter"],
                    format="tex" if TEX else "png",
                    show_missing=a["show_missing"],
                    matplotlib_options=MATPLOTLIB_OPTIONS
                ), replace=REPLACE),
                name=f"{algo1_name}-vs-{algo2_name}-{attribute}{suffix}",
            )

    add_generic_scatter(exp, "layer_size", "total_time",
                        scale="log", filter=filter_exclude_algs(excluded_algs))
    # add_generic_scatter(exp, "layer_size_literals", "clause_propagation_time")
    # add_generic_scatter(exp, "layer_size_literals", "extend_time")
    # add_generic_scatter(exp, "layer_size_literals", "path_construction_time")
    add_generic_scatter(exp, "layer_size_literals", "total_time",
                        scale="log", filter=filter_exclude_algs(excluded_algs))
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
