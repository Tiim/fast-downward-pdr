#! /usr/bin/env python

import logging
import re

from lab.parser import Parser


class CommonParser(Parser):
    def add_repeated_pattern(
        self, name, regex, file="run.log", required=False, type=int
    ):
        def find_all_occurences(content, props):
            matches = re.findall(regex, content)
            if required and not matches:
                logging.error(f"Pattern {regex} not found in file {file}")
            props[name] = [type(m) for m in matches]
            if type == int or type == float:
                props[name + "_total"] = sum(props[name])

        self.add_function(find_all_occurences, file=file)

    def add_bottom_up_pattern(
        self, name, regex, file="run.log", required=False, type=int
    ):
        def search_from_bottom(content, props):
            reversed_content = "\n".join(reversed(content.splitlines()))
            match = re.search(regex, reversed_content)
            if required and not match:
                logging.error(f"Pattern {regex} not found in file {file}")
            if match:
                props[name] = type(match.group(1))

        self.add_function(search_from_bottom, file=file)


def main():
    parser = CommonParser()
    parser.add_repeated_pattern(
        "layer_size",
        r"Layer size \d+: (\d+)\n",
        required=False,
        type=int,
    )
    parser.add_repeated_pattern(
        "layer_size_literals",
        r"Layer size \(literals\) \d+: (\d+)\n",
        required=False,
        type=int,
    )
    parser.add_repeated_pattern(
        "layer_size_seeded",
        r"Seed layer size \d+: (\d+)\n",
        required=False,
        type=int,
    )
    parser.add_pattern(
        "obligation_expansions",
        r"Total expanded obligations: (\d+)\n",
        required=False,
        type=int,
    )
    parser.add_pattern(
        "obligation_insertions",
        r"Total inserted obligations: (\d+)\n",
        required=False,
        type=int,
    )
    parser.add_pattern(
        "pattern_size",
        r"Pattern size: (\d+),",
        required=False,
        type=int,
    )
    parser.add_pattern(
        "clause_propagation_time",
        r"Total clause propagation time: (.*)s\n",
        required=False,
        type=float,
    )
    parser.add_pattern(
        "extend_time",
        r"Total extend time: (.*)s\n",
        required=False,
        type=float,
    )
    parser.add_pattern(
        "path_construction_time",
        r"Total path construction phase time: (.*)s\n",
        required=False,
        type=float,
    )
    parser.add_pattern(
        "layer_seed_time",
        r"Total seeding time: (.*)s\n",
        required=False,
        type=float,
    )
    parser.parse()


if __name__ == "__main__":
    main()
