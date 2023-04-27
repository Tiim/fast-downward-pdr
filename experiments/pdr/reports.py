from collections import defaultdict
import itertools
import logging
import math
import os

from downward.reports import PlanningReport
from downward.reports.scatter_matplotlib import ScatterMatplotlib
from downward.reports.scatter_pgfplots import ScatterPgfplots
from lab import tools


class ScatterPlotReport(PlanningReport):
    """
    Generate a scatter plot for an attribute.
    """

    def __init__(
        self,
        category_x,
        category_y,
        show_missing=True,
        get_category=None,
        title=None,
        scale=None,
        xlabel="",
        ylabel="",
        matplotlib_options=None,
        **kwargs,
    ):
        kwargs.setdefault("format", "png")
        kwargs.setdefault("attributes", [category_x, category_y])

        # Backwards compatibility.
        xscale = kwargs.pop("xscale", None)
        yscale = kwargs.pop("yscale", None)
        if xscale or yscale:
            logging.warning(
                'Use "scale" parameter instead of "xscale" and "yscale".')
        scale = scale or xscale or yscale

        PlanningReport.__init__(self, **kwargs)
        if len(self.attributes) != 2:
            logging.critical("ScatterPlotReport needs exactly two attributes")

        # By default all values are in the same category "None".
        self.get_category = get_category or (lambda run1: None)
        self.show_missing = show_missing
        if self.output_format == "tex":
            self.writer = ScatterPgfplots
        else:
            self.writer = ScatterMatplotlib
        self.title = title or ""
        self._set_scales(scale)
        self.xlabel = xlabel
        self.ylabel = ylabel
        # If the size has not been set explicitly, make it a square.
        self.matplotlib_options = matplotlib_options or {
            "figure.figsize": [8, 8]}
        if "legend.loc" in self.matplotlib_options:
            logging.warning('The "legend.loc" parameter is ignored.')

    def _set_scales(self, scale):
        self.xscale = scale or self.attributes[0].scale or "log"
        self.yscale = self.xscale
        scales = ["linear", "log", "symlog"]
        for scale in [self.xscale, self.yscale]:
            if scale not in scales:
                logging.critical(f"Scale {scale} not in {scales}")

    def has_multiple_categories(self):
        return any(key is not None for key in self.categories.keys())

    def _fill_categories(self):
        """Map category names to coordinate lists."""
        categories = defaultdict(list)
        for runs in self.problem_runs.values():
            for run in runs:
                category = self.get_category(run)
                c_x = run.get(self.attributes[0])
                c_y = run.get(self.attributes[1])
                if isinstance(c_x, list):
                    c_x = sum(c_x)
                if isinstance(c_y, list):
                    c_y = sum(c_y)
                coord = (c_x, c_y)
                if self.show_missing or None not in coord:
                    categories[category].append(coord)
        return categories

    def _compute_missing_value(self, categories, axis, scale):
        if not self.show_missing:
            return None
        values = [coord[axis] for coords in categories.values()
                  for coord in coords]
        real_values = [value for value in values if value is not None]
        if len(real_values) == len(values):
            # The list doesn't contain None values.
            return None
        if not real_values:
            return 1
        max_value = max(real_values)
        if scale == "linear":
            return max_value * 1.1
        return int(10 ** math.ceil(math.log10(max_value)))

    def _handle_non_positive_values(self, categories):
        """Plot integer 0 values at as None in log plots and abort if any value is < 0."""
        assert self.xscale == self.yscale == "log"
        new_categories = {}
        for category, coords in categories.items():
            new_coords = []
            for x, y in coords:
                if x == 0 and isinstance(x, int):
                    x = None
                elif x <= 0 and isinstance(x, float):
                    x += 0.00001

                if y == 0 and isinstance(y, int):
                    y = None
                elif y <= 0:
                    y += 0.00001

                if (x is not None and x <= 0) or (y is not None and y <= 0):
                    logging.critical(
                        "Logarithmic axes can only show positive values. "
                        "Use a symlog or linear scale instead."
                        f"x: {x}, y: {y}"
                    )
                else:
                    new_coords.append((x, y))
            new_categories[category] = new_coords
        return new_categories

    def _handle_missing_values(self, categories):
        x_missing = self._compute_missing_value(categories, 0, self.xscale)
        y_missing = self._compute_missing_value(categories, 1, self.yscale)
        if x_missing is None:
            missing_value = y_missing
        elif y_missing is None:
            missing_value = x_missing
        else:
            missing_value = max(x_missing, y_missing)
        self.x_upper = missing_value
        self.y_upper = missing_value

        if not self.show_missing:
            # Coords with None values have already been filtered.
            return categories

        new_categories = {}
        for category, coords in categories.items():
            coords = [
                (
                    x if x is not None else missing_value,
                    y if y is not None else missing_value,
                )
                for x, y in coords
            ]
            if coords:
                new_categories[category] = coords
        return new_categories

    def _get_category_styles(self, categories):
        """
        Create dictionary mapping from category name to marker style.
        """
        shapes = "x+os^v<>D"
        colors = [f"C{c}" for c in range(10)]

        num_styles = len(shapes) * len(colors)
        styles = [
            {"marker": shape, "c": color}
            for shape, color in itertools.islice(
                zip(itertools.cycle(shapes), itertools.cycle(colors)), num_styles
            )
        ]
        assert (
            len({(s["marker"], s["c"]) for s in styles}) == num_styles
        ), "The number of shapes and the number of colors must be coprime."

        category_styles = {}
        for i, category in enumerate(sorted(categories)):
            category_styles[category] = styles[i % len(styles)]
        return category_styles

    def _get_axis_label(self, label, algo):
        if label:
            return label
        return f"{algo}"

    def _write_plot(self, runs, filename):
        # Map category names to coord tuples.
        self.categories = self._fill_categories()

        self.plot_diagonal_line = False
        self.plot_horizontal_line = False
        if self.xscale == "log":
            assert self.yscale == "log"
            self.categories = self._handle_non_positive_values(self.categories)
        self.categories = self._handle_missing_values(self.categories)

        if not self.categories:
            logging.critical("Plot contains no points.")

        self.xlabel = self._get_axis_label(
            self.xlabel, self.attributes[0])
        self.ylabel = self._get_axis_label(
            self.ylabel, self.attributes[1])

        self.styles = self._get_category_styles(self.categories)
        self.writer.write(self, filename)

    def write(self):
        suffix = "." + self.output_format
        if not self.outfile.endswith(suffix):
            self.outfile += suffix
        tools.makedirs(os.path.dirname(self.outfile))
        self._write_plot(self.runs.values(), self.outfile)
