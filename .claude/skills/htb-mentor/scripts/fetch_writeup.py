#!/usr/bin/env python3
"""
Fetch HackTheBox writeup from 0xdf.gitlab.io or other sources.

This script attempts to find and return the URL for a specific HackTheBox
machine writeup, prioritizing 0xdf.gitlab.io but falling back to web search
if needed.
"""

import sys
import re
from urllib.parse import quote


def construct_0xdf_url(box_name):
    """
    Construct the expected 0xdf.gitlab.io URL for a box.
    
    0xdf typically uses lowercase box names in URLs, e.g.:
    https://0xdf.gitlab.io/2020/06/27/htb-cache.html
    
    However, we need to search since we don't know the date pattern.
    """
    # Normalize box name to lowercase for searching
    normalized_name = box_name.lower().strip()
    return f"https://0xdf.gitlab.io", normalized_name


def main():
    if len(sys.argv) < 2:
        print("Usage: fetch_writeup.py <box-name>", file=sys.stderr)
        sys.exit(1)
    
    box_name = sys.argv[1]
    base_url, normalized_name = construct_0xdf_url(box_name)
    
    # Return search guidance for Claude to use web_search tool
    print(f"SEARCH_REQUIRED")
    print(f"PRIMARY: site:0xdf.gitlab.io {box_name} HackTheBox")
    print(f"FALLBACK: {box_name} HackTheBox writeup")


if __name__ == "__main__":
    main()
