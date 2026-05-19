---
name: md2paper
description: Convert Markdown files to academic paper-style Word (.docx) and PDF documents. Dual format support: Chinese academic paper (paperol_cn_v2.0 template) and English arXiv-style.
---

# Markdown to Academic Paper Converter

Convert Markdown files to properly formatted Word documents (and optionally PDF). Auto-detects language from filename (Chinese characters = CN format, otherwise EN/arXiv format).

## When to Use

- User asks to convert `.md` files to `.docx` / Word / paper / PDF format
- User wants to generate academic papers, white papers, or theory documents from Markdown
- User references paper formatting standards (Chinese academic / arXiv style)

## Prerequisites

```bash
pip install python-docx lxml docx2pdf
```

## Usage

```bash
# Word only
python .claude/skills/md2paper/md2docx.py <input.md> <output.docx>

# Word + PDF
python .claude/skills/md2paper/md2docx.py <input.md> <output.docx> --pdf
```

## arXiv Format Reference (learned from 2604.21804v1.pdf)

### Title & Author Block

```
Phenomenological Detector Design and Optimization in Vertically-Integrated...
                                      (centered, normal weight, ~14-16pt)

Wonyong Chung                        (name)
Princeton University                 (affiliation)
Jadwin Hall, Washington Road, ...   (address)
wonyongc@princeton.edu               (email)

Qibin Liu                            (next author, same pattern)
SLAC National Accelerator Laboratory
...
```

### Abstract Block

```
Abstract                                  ("Abstract" title case, bold, standalone)
We present the first implementation...    (flush-left, NO indent, single paragraph)
```

### Body Text

```
1 Introduction                            (decimal section numbering)
High-energy physics (HEP) research...     (first-line indent, justified)
2.1 Bilevel optimization...               (hierarchical numbering)
```

### Figure & Table Captions

```
Figure 1: Bilevel detector optimization... (full "Figure", bold colon separator, centered below)

Table 1: Optimization parameters...        (full "Table", bold colon separator, centered above)
```

## Format Specifications

### Chinese Format (paperol_cn_v2.0)

| Element | Font | Size | Style |
|---------|------|------|-------|
| Title | SimSun | 16pt | Bold, centered |
| Author | SimSun | 12pt | Centered, NO prefix |
| Body | SimSun + TNR | 10.5pt (五号) | Normal, first-line indent 0.74cm |
| Heading 1 | SimSun + TNR | 14pt (三号) | Bold |
| Heading 2 | SimSun + TNR | 12pt (小三) | Bold |
| Heading 3 | SimSun + TNR | 10.5pt (五号) | Bold |
| Abstract | SimSun + TNR | 10.5pt | Flush-left, bold label ("摘要：" + content) |
| Keywords | SimSun + TNR | 10.5pt | Flush-left, bold label ("关键词：" + keywords) |
| Table cells | SimSun + TNR | 9pt | Light blue header bg |
| Code blocks | Consolas | 9pt | Gray bg (#F5F5F5) + border |
| Blockquote | SimSun + TNR | 10.5pt | Italic, blue left border |
| Caption | SimSun + TNR | 9pt | Centered |

### English Format (arXiv-style, from 2604.21804v1.pdf)

| Element | Font | Size | Style |
|---------|------|------|-------|
| Title | Times New Roman | 16pt | Centered, normal weight |
| Author | Times New Roman | 11pt | Centered, NO prefix (name, affiliation, address, email stacked) |
| Body | Times New Roman | 10pt | Normal, first-line indent ~0.3cm, justified |
| Heading 1 | Times New Roman | 14pt | Bold, decimal numbering (1, 2, 3) |
| Heading 2 | Times New Roman | 12pt | Bold, hierarchical (2.1, 2.2) |
| Heading 3 | Times New Roman | 10pt | Bold, multi-level (2.1.1) |
| Abstract header | Times New Roman | 10pt | "Abstract" (title case), bold, standalone line |
| Abstract content | Times New Roman | 10pt | Flush-left (NO indent), single paragraph |
| Abstract labels | Times New Roman | 10pt | Bold inline keywords: "Context.", "Aims.", "Methods.", "Results.", "Conclusions." |
| Keywords | Times New Roman | 10pt | "Key words." bold prefix, flush-left |
| Table cells | Times New Roman | 9pt | Light blue header bg |
| Table caption | Times New Roman | 10pt | Centered, "Table N: " (full word, bold colon) |
| Figure caption | Times New Roman | 10pt | Centered, "Figure N: " (full "Figure", bold colon) |
| Code blocks | Consolas | 9pt | Gray bg + border |
| Blockquote | Times New Roman | 10pt | Italic, blue left border |
| Reference citations | Times New Roman | 10pt | Superscript [1], [2], blue |
| Acknowledgments | Times New Roman | 10pt | Bold standalone heading |

## Page Layout

| | Chinese | English (arXiv) |
|--|---------|-----------------|
| Margins | T/B 2.54cm, L/R 3.17cm | 2.54cm all sides |
| Line spacing | 16pt | 14.5pt |
| First-line indent | 0.74cm | ~0.3cm (standard indent) |
| Abstract indent | NO indent (flush-left) | NO indent (flush-left) |
| Text alignment | Both justified | Both justified |

## Abstract & Keywords Layout

### Chinese
- `摘要：` (bold label) + content paragraph, flush-left
- `关键词：` (bold label) + keywords line, flush-left

### English (arXiv)
- Header: `Abstract` (title case, bold, standalone line)
- Content: flush-left, NO left indentation, single paragraph
- Structure: inline bold keywords + text → "Context. ... Aims. ... Methods. ... Results. ... Conclusions. ..."
- Keywords: `Key words.` (bold) + keyword list, flush-left

### Content parsing rules
- Mixed text + markdown tables → text paragraphs + real Word tables
- Empty lines separate text blocks
- Horizontal rules (`---`) skipped

## Table/Figure Captions

**Tables** — caption ABOVE table:
- Chinese: `表1  名称`
- English: `Table 1: Name` (full word "Table", bold colon separator)

**Figures** — caption below figure:
- Chinese: `图1  名称`
- English: `Figure 1: Name` (full "Figure" word, NOT abbreviated "Fig.", bold colon separator)

## Reference Citations

Inline `[1]`, `[2]` etc. → blue superscript, Times New Roman.

## Page Numbers

Centered in footer, PAGE field only.

## Auto-processing

- **YAML frontmatter**: Parsed for `author` metadata, then stripped
- **Author rendering**: Title → Author (centered, NO prefix label) → content
- **Author dedup**: If plain-text author line matches YAML author, skipped
- **Author fallback**: If no YAML author, first non-empty line after title extracted as author
- **Blockquote stripping**: All `>` lines after title removed
- **Footer removal**: `## Software Citation` / `## 软件引用` section and `*Version:*` / `*Date:*` / `*Author:*` footer stripped
- **Language detection**: Filename containing Chinese characters → CN format, else EN format

## Supported Markdown

- Headings (# through ######)
- Fenced code blocks (```)
- Tables with header separator → real Word tables
- Unordered lists (-, *, +)
- Ordered lists (1., 2., etc.)
- Blockquotes (>)
- Inline bold (**) and italic (*)
- Images (![alt](url))
- Horizontal rules (---)
- Reference citations ([1], [2])
- YAML frontmatter (metadata extracted, block stripped)
