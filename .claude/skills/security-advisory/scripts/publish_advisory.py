#!/usr/bin/env python3
"""Create (and by default publish) a MidnightBSD GitHub security advisory.

Finds the next free MNBSD-YYYY-N number, builds the request the way the
GitHub API wants it (ecosystem "other", CVSS vector without severity, draft
first then publish) and prints the resulting advisory URL.

Requires an authenticated gh(1).
"""

import argparse
import datetime
import json
import re
import subprocess
import sys

REPO = "MidnightBSD/src"


def gh_api(*args, data=None):
    cmd = ["gh", "api"] + list(args)
    out = subprocess.run(cmd, input=data, capture_output=True, text=True)
    if out.returncode != 0:
        sys.exit("gh api failed: %s\n%s" % (" ".join(args), out.stderr.strip()))
    return json.loads(out.stdout) if out.stdout.strip() else None


def existing_advisories():
    pages = gh_api("--paginate", "--slurp",
                   "repos/%s/security-advisories" % REPO) or []
    return [adv for page in pages for adv in page]


def next_number(advisories, year):
    pat = re.compile(r"^MNBSD-%d-(\d+)\b" % year)
    nums = [int(m.group(1)) for a in advisories
            for m in [pat.match(a.get("summary", ""))] if m]
    return (max(nums) + 1) if nums else 1


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--title", required=True,
                   help="short title, without the MNBSD number or CVE")
    p.add_argument("--cve", action="append", default=[],
                   help="CVE id; repeat for several (first one is the primary)")
    p.add_argument("--description", required=True,
                   help="Markdown file with the advisory body")
    p.add_argument("--cvss", help="CVSS 3.1 vector string")
    p.add_argument("--severity", choices=["low", "medium", "high", "critical"],
                   help="only when no CVSS vector is given")
    p.add_argument("--cwe", action="append", default=[], help="CWE-NNN; repeatable")
    p.add_argument("--package", default="kernel",
                   help="package name, e.g. kernel/sysvsem or openssl")
    p.add_argument("--patched", required=True,
                   help="first stable/4.0 release containing the fix, e.g. 4.0.8")
    p.add_argument("--year", type=int, default=datetime.date.today().year)
    p.add_argument("--number", type=int, help="override the MNBSD number")
    p.add_argument("--draft", action="store_true", help="create but do not publish")
    a = p.parse_args()

    if a.cvss and a.severity:
        sys.exit("the API rejects severity together with a CVSS vector; give one")
    if not a.cvss and not a.severity:
        sys.exit("give --cvss or --severity")

    advisories = existing_advisories()
    for cve in a.cve:
        for adv in advisories:
            if adv.get("cve_id") == cve or cve in adv.get("summary", ""):
                sys.exit("%s already has an advisory: %s (%s)"
                         % (cve, adv["summary"], adv["html_url"]))
    number = a.number or next_number(advisories, a.year)
    cves = ", ".join(a.cve)
    summary = "MNBSD-%d-%d %s" % (a.year, number, a.title)
    if cves:
        summary += " (%s)" % cves

    with open(a.description) as f:
        description = f.read()

    body = {
        "summary": summary,
        "description": description,
        "cwe_ids": a.cwe,
        "vulnerabilities": [{
            "package": {"ecosystem": "other", "name": a.package},
            "vulnerable_version_range": "< %s" % a.patched,
            "patched_versions": a.patched,
        }],
    }
    if a.cve:
        body["cve_id"] = a.cve[0]
    if a.cvss:
        body["cvss_vector_string"] = a.cvss
    else:
        body["severity"] = a.severity

    created = gh_api("-X", "POST", "repos/%s/security-advisories" % REPO,
                     "--input", "-", data=json.dumps(body))
    ghsa = created["ghsa_id"]
    print("created %s: %s" % (ghsa, summary))
    if a.draft:
        print("draft: %s" % created["html_url"])
        return
    pub = gh_api("-X", "PATCH", "repos/%s/security-advisories/%s" % (REPO, ghsa),
                 "-f", "state=published")
    print("%s severity=%s %s" % (pub["state"], pub.get("severity"), pub["html_url"]))


if __name__ == "__main__":
    main()
