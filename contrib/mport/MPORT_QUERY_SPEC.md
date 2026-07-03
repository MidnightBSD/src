# mport query specification

`mport query` prints installed package metadata using a FreeBSD `pkg query` style
format string. The command is backed by reusable `libmport` query helpers so
other mport tools can share the same matching, filtering, and formatting logic.

## Command forms

- `mport query <format>` prints all installed packages.
- `mport query -a <format>` explicitly prints all installed packages.
- `mport query <format> <pkg-name> ...` prints exact package-name matches.
- `mport query [-Cgix] <format> <pattern> ...` selects case-sensitive,
  glob, case-insensitive, or regular-expression matching.
- `mport query -e <expression> <format> [pattern ...]` filters matched
  packages with a simple expression.

`-F <pkg-file>` is intentionally deferred. v1 only queries the installed package
database.

## Supported format fields

Scalar fields:

- `%n`: package name
- `%v`: installed version
- `%o`: origin
- `%p`: prefix
- `%m`: maintainer annotation
- `%c`: comment
- `%e`: description, when available
- `%w`: WWW annotation
- `%a`: automatic flag, `1` or `0`
- `%k`: locked flag, `1` or `0`
- `%t`: install timestamp
- `%s` or `%sb`: flat size in bytes
- `%sh` or `%h`: human-readable flat size
- `%M`: installed package message
- `%X`: index hash, when an index is loaded
- `%l`: license metadata, when an index is loaded
- `%q`: target architecture
- `%P`: package URL

List fields are printed as comma-separated values:

- `%A`: annotations as `tag=value`
- `%C`: categories
- `%D`: registered directories
- `%F`: registered files
- `%L`: licenses split from license metadata
- `%O`: options split from the stored options metadata
- `%d`: dependencies
- `%r`: reverse dependencies

`%#<list>` prints a supported list count and `%?<list>` prints `1` when the list
is present or `0` when it is empty. Unknown format codes are errors.

## Expressions

The v1 expression engine supports scalar checks over the same scalar format
fields:

- truth checks, such as `%k`
- equality and inequality with `=` and `!=`
- glob matching with `~`
- version comparisons with `<`, `<=`, `>`, and `>=`
- boolean `&&`, `||`, and leading `!`

Parentheses and package-file expressions are not part of v1.

## Compatibility notes

The implementation intentionally keeps the existing `libexec/mport.query`
predicate interface unchanged. Unsupported FreeBSD `pkg query` features should
be documented instead of silently producing misleading output.
