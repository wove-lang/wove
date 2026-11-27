# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Collect supplied Wove sources and embeds for the seed-built native emitter."""

import os
from pathlib import Path

from syntax import Atom, Form, SourceError, parse


# Follow the repository's declared imports and embeds in first-encounter order.
def collect_inputs(root, entries, *, embedded_providers=False, allow_missing_imports=False):
	sources = {}
	assets = {}
	needs_bundle = False

	# Visit each normalized source path once before descending through its uses.
	def visit(path, *, entry=False):
		path = os.path.normpath(path)
		# A shared dependency contributes its source and assets once.
		if path in sources:
			return
		# The fixture's native resolver diagnoses missing imports at their original uses.
		try:
			source = (root / path).read_text(encoding="utf-8")
		except FileNotFoundError:
			# The fixed compiler graph and fixture entry are always required.
			if entry or not allow_missing_imports:
				raise
			return
		sources[path] = None
		# Annotate syntax errors from a supplied file with its defining path.
		try:
			node = parse(source)
		except SourceError as error:
			raise SourceError(error.line, path) from None
		walk(node, Path(path).parent)

	# Only Wove imports and embed forms contribute paths to the supplied graph.
	def walk(node, parent):
		nonlocal needs_bundle
		# Atoms and disabled forms cannot contribute candidate inputs.
		if not isinstance(node, Form) or node.tag == "disabled":
			return
		# Leave malformed operands to the native checker.
		if node.tag == "use" and len(node.operands) >= 2:
			provider, operand = node.operands[:2]
			# Import paths are quoted and relative to their defining source.
			if isinstance(provider, Atom) and not provider.quoted and isinstance(operand, Atom) and operand.quoted:
				# Wove modules and bundled C providers enter the supplied table by different paths.
				if provider.text == "wove-file":
					visit(parent / operand.text)
				elif provider.text == "c-header":
					needs_bundle = True
		# Embeds contribute asset bytes under the same virtual source-relative path.
		elif node.tag == "embed" and len(node.operands) == 1:
			operand, = node.operands
			# Only a quoted operand names an asset.
			if isinstance(operand, Atom) and operand.quoted:
				assets[os.path.normpath(parent / operand.text)] = None
		# Imports and embeds can occur inside declaration groups and function bodies.
		else:
			# Walk ordinary operands without interpreting metadata or cfg selection.
			for child in node.operands:
				walk(child, parent)

	# Keep the designated root first in each supplied graph.
	for entry in entries:
		visit(entry, entry=True)
	# The fixed compiler embeds its C providers as both bytes and declarations.
	if embedded_providers:
		# Ordinary embeds remain assets regardless of their filename suffix.
		for path in tuple(assets):
			# Only the compiler's embedded Wove modules supply declarations.
			if Path(path).suffix == ".wove":
				visit(path)
	return sources, assets, needs_bundle
