from mcp.server.fastmcp import FastMCP
import pathlib
import re

mcp = FastMCP("livekit-sdk-docs")

DOC_PATH = pathlib.Path("./clients/web/docs/docs.livekit.io/reference/client-sdk-js")

HTML_TAG = re.compile(r"<[^>]+>")
TITLE_RE = re.compile(r"<title>(.*?)</title>", re.I | re.S)
H1_RE = re.compile(r"<h1.*?>(.*?)</h1>", re.I | re.S)


def clean_html(text: str):
    text = re.sub(r"<script.*?</script>", "", text, flags=re.S)
    text = re.sub(r"<style.*?</style>", "", text, flags=re.S)
    text = HTML_TAG.sub(" ", text)
    text = re.sub(r"\s+", " ", text)
    return text.strip()


def extract_title(html: str):
    m = TITLE_RE.search(html)
    if m:
        return clean_html(m.group(1))

    m = H1_RE.search(html)
    if m:
        return clean_html(m.group(1))

    return "unknown"


# ---------- build index at startup ----------

DOC_INDEX = []

for file in DOC_PATH.rglob("*.html"):
    try:
        raw = file.read_text(encoding="utf-8", errors="ignore")
        title = extract_title(raw)

        DOC_INDEX.append({
            "path": str(file.relative_to(DOC_PATH)),
            "title": title,
            "content": clean_html(raw)
        })

    except Exception:
        pass


# ---------- tools ----------

@mcp.tool()
def search_sdk_docs(query: str):
    """
    Search LiveKit JS SDK documentation.
    """
    q = query.lower()
    results = []

    for doc in DOC_INDEX:
        if q in doc["content"].lower() or q in doc["title"].lower():

            idx = doc["content"].lower().find(q)
            snippet = doc["content"][max(0, idx-200):idx+300]

            results.append({
                "title": doc["title"],
                "file": doc["path"],
                "snippet": snippet
            })

        if len(results) >= 8:
            break

    return results


@mcp.tool()
def read_doc(path: str):
    """
    Read a full documentation page.
    """
    file = DOC_PATH / path
    raw = file.read_text(encoding="utf-8", errors="ignore")

    return {
        "path": path,
        "content": clean_html(raw)[:12000]
    }


@mcp.tool()
def list_sdk_sections():
    """
    List available documentation sections.
    """
    sections = {}

    for doc in DOC_INDEX:
        section = doc["path"].split("/")[0]
        sections.setdefault(section, 0)
        sections[section] += 1

    return sections


if __name__ == "__main__":
    mcp.run()