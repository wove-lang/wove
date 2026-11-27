#!/usr/bin/env python3
# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

import json
import os
from pathlib import Path
import sys


# Record the original argv and apply the output, streams and exit specified by the fixture.
def main(args):
	output = Path(args[args.index("-o") + 1])
	with output.with_suffix(output.suffix + ".argv").open("x", encoding="utf-8") as stream:
		json.dump(args, stream)
	actions = json.loads(os.environ["WOVE_STAGED_ACTIONS"])
	action = actions[output.name]
	sys.stdout.write(action.get("stdout", ""))
	sys.stderr.write(action.get("stderr", ""))
	# A nonzero status may accompany a partially produced output.
	if "output" in action:
		output.write_text(action["output"], encoding="utf-8")
	return action.get("exit", 0)


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
