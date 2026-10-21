#!/usr/bin/env python3
import argparse
import json
import re
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'web'
OUTPUT = ROOT / 'data'
STATIC_FILES = ('index.html', 'app.js', 'style.css', 'gpio-pins.json')
CONFIG_EXAMPLE = SOURCE / 'config.example.json'
CONFIG_LOCAL = SOURCE / 'config.local.json'


def minify_html(text):
    text = re.sub(r'<!--(?!\[if)[\s\S]*?-->', '', text)
    return re.sub(r'>\s+<', '><', text).strip()


def minify_css(text):
    text = re.sub(r'/\*[\s\S]*?\*/', '', text)
    text = re.sub(r'\s+', ' ', text)
    return re.sub(r'\s*([{}:;,])\s*', r'\1', text).strip()


def minify_js(text):
    output = []
    index = 0
    quote = None
    escaped = False
    line_comment = False
    block_comment = False
    pending_space = False
    while index < len(text):
        char = text[index]
        next_char = text[index + 1] if index + 1 < len(text) else ''
        if line_comment:
            if char in '\r\n':
                line_comment = False
            index += 1
            continue
        if block_comment:
            if char == '*' and next_char == '/':
                block_comment = False
                index += 2
            else:
                index += 1
            continue
        if quote:
            output.append(char)
            if escaped:
                escaped = False
            elif char == '\\':
                escaped = True
            elif char == quote:
                quote = None
            index += 1
            continue
        if char in ('"', "'", '`'):
            if pending_space and output and (output[-1][-1].isalnum() or output[-1][-1] in '_$'):
                output.append(' ')
            pending_space = False
            quote = char
            output.append(char)
            index += 1
            continue
        if char == '/' and next_char == '/':
            line_comment = True
            index += 2
            continue
        if char == '/' and next_char == '*':
            block_comment = True
            index += 2
            continue
        if char.isspace():
            pending_space = True
            index += 1
            continue
        if pending_space and output:
            previous = output[-1][-1]
            if (previous.isalnum() or previous in '_$') and (char.isalnum() or char in '_$'):
                output.append(' ')
        pending_space = False
        output.append(char)
        index += 1
    return ''.join(output).strip()


def minify_json(text):
    return json.dumps(json.loads(text), separators=(',', ':'))


def bootstrap():
    if SOURCE.exists():
        return
    SOURCE.mkdir(parents=True)
    for name in STATIC_FILES:
        source = OUTPUT / name
        if source.exists():
            shutil.copy2(source, SOURCE / name)
    print('Created web/ from current data/ assets. Format sources in web/ before editing.')


def build():
    if not SOURCE.exists():
        raise SystemExit('web/ does not exist. Run with --bootstrap once.')
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for name in STATIC_FILES:
        source = SOURCE / name
        if not source.exists():
            raise SystemExit(f'Missing source asset: {source}')
        text = source.read_text(encoding='utf-8')
        if name.endswith('.html'):
            text = minify_html(text)
        elif name.endswith('.css'):
            text = minify_css(text)
        elif name.endswith('.js'):
            text = minify_js(text)
        elif name.endswith('.json'):
            text = minify_json(text)
        destination = OUTPUT / name
        destination.write_text(text, encoding='utf-8', newline='\n')
        source_size = source.stat().st_size
        output_size = destination.stat().st_size
        print(f'Built data/{name}: {source_size} -> {output_size} bytes')
    config_source = CONFIG_LOCAL if CONFIG_LOCAL.exists() else CONFIG_EXAMPLE
    if not config_source.exists():
        raise SystemExit(f'Missing config template: {CONFIG_EXAMPLE}')
    config_text = minify_json(config_source.read_text(encoding='utf-8'))
    config_destination = OUTPUT / 'config.json'
    config_destination.write_text(config_text, encoding='utf-8', newline='\n')
    print(f'Built data/config.json from {config_source.relative_to(ROOT)}')


parser = argparse.ArgumentParser()
parser.add_argument('--bootstrap', action='store_true')
args = parser.parse_args()
if args.bootstrap:
    bootstrap()
build()
