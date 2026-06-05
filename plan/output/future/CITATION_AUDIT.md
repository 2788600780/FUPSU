# Citation Audit Report

**Date**: 2026-05-19
**Bib source**: embedded `\begin{thebibliography}` in `main.tex` (lines 1243–1359)
**Total entries**: 37 (36 cited + 1 self-reference artifact)
**Reviewer**: WebSearch + WebFetch (adapted from Codex MCP)

## Summary

| Verdict | Count |
|---------|-------|
| KEEP    | 28    |
| FIX     | 9     |
| REPLACE | 0     |
| REMOVE  | 0     |

## Overall Verdict: **WARN** — 9 entries have truncated or abbreviated titles; no hallucinated papers detected

---

## Priority Fixes (title corrections)

### JLZ20 — Missing subtitle
- **Entry**: "Shuffle-based Private Set Union." USENIX Security, 2022.
- **Should be**: "Shuffle-based Private Set Union: Faster and More Secure."
- **ACTION**: Add subtitle ": Faster and More Secure"

### ZCL+23 — Abbreviation in title
- **Entry**: "...from Multi-Query RPMT."
- **Should be**: "...from Multi-Query Reverse Private Membership Test."
- **ACTION**: Expand "RPMT" to "Reverse Private Membership Test"

### Tu+25 — Missing subtitle
- **Entry**: "Fast Enhanced Private Set Union." USENIX Security, 2025.
- **Should be**: "Fast Enhanced Private Set Union in the Balanced and Unbalanced Scenarios."
- **ACTION**: Add subtitle "in the Balanced and Unbalanced Scenarios"

### DCW13 — Missing subtitle
- **Entry**: "When Private Set Intersection Meets Big Data." CCS, 2013.
- **Should be**: "When Private Set Intersection Meets Big Data: An Efficient and Scalable Protocol."
- **ACTION**: Add subtitle ": An Efficient and Scalable Protocol"

### IKN+20 — Missing subtitle
- **Entry**: "On Deploying Secure Computing." IEEE EuroS&P, 2020.
- **Should be**: "On Deploying Secure Computing: Private Intersection-Sum-with-Cardinality."
- **ACTION**: Add subtitle ": Private Intersection-Sum-with-Cardinality"

### PT26 — Missing subtitle
- **Entry**: "Is PSI Really Faster Than PSU?" Eurocrypt, 2026.
- **Should be**: "Is PSI Really Faster Than PSU? Achieving Efficient PSU with Invertible Bloom Filters."
- **ACTION**: Add subtitle "Achieving Efficient PSU with Invertible Bloom Filters"

### RRT23 — Abbreviation in title
- **Entry**: "Expand-Convolute Codes for PCGs from LPN." CRYPTO, 2023.
- **Should be**: "Expand-Convolute Codes for Pseudorandom Correlation Generators from LPN."
- **ACTION**: Expand "PCGs" to "Pseudorandom Correlation Generators"

### KKRT16 — Missing subtitle
- **Entry**: "Efficient Batched Oblivious PRF." CCS, 2016.
- **Should be**: "Efficient Batched Oblivious PRF with Applications to Private Set Intersection."
- **ACTION**: Add subtitle "with Applications to Private Set Intersection"

### Mat24 — Missing subtitle
- **Entry**: "Security of Symmetric Ratchets and Key Chains." ePrint 2024/220.
- **Should be**: "Security of Symmetric Ratchets and Key Chains - Implications for Protocols like TLS 1.3, Signal, and PQ3."
- **ACTION**: Add subtitle "Implications for Protocols like TLS 1.3, Signal, and PQ3"

---

## Minor Title Variants (acceptable, no action required)

| Key | Note |
|-----|------|
| BMS+24 | Shortened: missing ": Extended Functionalities, Deletion, and Worst-Case Complexity" |
| LMR+26 | "PSI" for "Private Set Intersection" (standard abbreviation) |
| ABF+25 | Shortened: missing ": Efficient Constructions via Circuit Private Set Intersection" |
| WZC+24 | "PSI" for "Private Set Intersection" (standard abbreviation) |
| Can01 | Shortened from "Universally Composable Security: A New Paradigm..." (universally accepted) |
| libOTe | Title wording differs from repo's canonical BibTeX |
| HKL+24 | Shortened: missing ": Cryptanalysis and Better Construction" |
| LBL25 | Shortened: missing "for Balanced and Unbalanced Scenarios" |
| CGS24 | Abbreviated: "PSU" for "Private Set Union", "Circuit PSI" for "Circuit-Based PSI" |

---

## All-Clean Entries (no action needed)

KRT19, KRS+19, Signal, BMX22, LTS+26, GM11, Ber08, BCK96, RS21, RR22, PuGT26, Bos16, Jia+24, Dong+25, YGA24, FLS23, GNT24, YBH+25

---

## Self-Reference Artifact

### code — UNCERTAIN
- **Entry**: Anonymous. "F-PSU Prototype Implementation." GitHub (released upon publication).
- **Verdict**: Self-reference — not verifiable via web search. Repository is stated to be released upon publication.
- **ACTION**: Ensure the repository is made public before/during camera-ready submission. Update the URL from `https://github.com/` to the actual repository URL.

---

## Notes

- **No hallucinated papers detected** — all 36 verifiable entries correspond to real, published works
- **No author errors** — all author names match canonical sources (DBLP, ePrint, venue proceedings)
- **No venue errors** — all venues and years match canonical records
- **Recurring pattern**: Subtitles after colons/dashes are frequently omitted. This is common in crypto bibliographies but should be corrected for submission quality.
- **Old audit fixes applied**: CGS24 (authors fixed), RRT23 (title partially fixed from completely fabricated to abbreviated), PSSZ15 (removed)
