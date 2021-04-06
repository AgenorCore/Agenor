#!/usr/bin/env python3
# Copyright (c) 2014 Wladimir J. van der Laan
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
'''
Run this script from the root of the repository to update all translations from
transifex.
It will do the following automatically:

- fetch all translations using the tx tool
- post-process them into valid and committable format
  - remove invalid control characters
  - remove location tags (makes diffs less noisy)
- update git for added translations
- update build system
'''
import argparse
import subprocess
import re
import sys
import os
import io
import xml.etree.ElementTree as ET

# Name of transifex tool
TX = 'tx'
# Name of source language file
SOURCE_LANG = 'agenor_en.ts'
# Directory with locale files
LOCALE_DIR = 'src/qt/locale'
# Minimum number of messages for translation to be considered at all
MIN_NUM_MESSAGES = 10
# Minimum completion percentage required to download from transifex
MINIMUM_PERC = 80
# Path to git
GIT = os.getenv("GIT", "git")

def check_at_repository_root():
    if not os.path.exists('.git'):
        print('No .git directory found')
        print('Execute this script at the root of the repository', file=sys.stderr)
        sys.exit(1)

def remove_current_translations():
    '''
    Remove current translations, as well as temporary files that might be left behind
    We only want the active translations that are currently on transifex.
    This leaves agenor_en.ts untouched.
    '''
    for (_,name) in all_ts_files():
        os.remove(name)
    for (_,name) in all_ts_files('.orig'):
        os.remove(name + '.orig')

def fetch_all_translations(fAll = False):
    call_list = [TX, 'pull', '-f', '-a']
    if not fAll:
        call_list.append('--minimum-perc=%s' % MINIMUM_PERC)
    if subprocess.call(call_list):
        print('Error while fetching translations', file=sys.stderr)
        sys.exit(1)

def find_format_specifiers(s):
    '''Find all format specifiers in a string.'''
    pos = 0
    specifiers = []
    while True:
        percent = s.find('%', pos)
        if percent < 0:
            break
        try:
            specifiers.append(s[percent+1])
        except:
            print('Failed to get specifier')
        pos = percent+2
    return specifiers

def split_format_specifiers(specifiers):
    '''Split format specifiers between numeric (Qt) and others (strprintf)'''
    numeric = []
    other = []
    for s in specifiers:
        if s in {'1','2','3','4','5','6','7','8','9'}:
            numeric.append(s)
        else:
            other.append(s)

    # If both numeric format specifiers and "others" are used, assume we're dealing
    # with a Qt-formatted message. In the case of Qt formatting (see https://doc.qt.io/qt-5/qstring.html#arg)
    # only numeric formats are replaced at all. This means "(percentage: %1%)" is valid, without needing
    # any kind of escaping that would be necessary for strprintf. Without this, this function
    # would wrongly detect '%)' as a printf format specifier.
    if numeric:
        other = []

    # numeric (Qt) can be present in any order, others (strprintf) must be in specified order
    return set(numeric),other

def sanitize_string(s):
    '''Sanitize string for printing'''
    return s.replace('\n',' ')

def check_format_specifiers(source, translation, errors, numerus):
    source_f = split_format_specifiers(find_format_specifiers(source))
    # assert that no source messages contain both Qt and strprintf format specifiers
    # if this fails, go change the source as this is hacky and confusing!
    assert(not(source_f[0] and source_f[1]))
    try:
        translation_f = split_format_specifiers(find_format_specifiers(translation))
    except IndexError:
        errors.append("Parse error in translation for '%s': '%s'" % (sanitize_string(source), sanitize_string(translation)))
        return False
    else:
        if source_f != translation_f:
            if numerus and source_f == (set(), ['n']) and translation_f == (set(), []) and translation.find('%') == -1:
                # Allow numerus translations to omit %n specifier (usually when it only has one possible value)
                return True
            errors.append("Mismatch between '%s' and '%s'" % (sanitize_string(source), sanitize_string(translation)))
            return False
    return True

def all_ts_files(suffix='', include_source=False):
    for filename in os.listdir(LOCALE_DIR):
        # process only language files, and do not process source language
        if not filename.endswith('.ts'+suffix) or (not include_source and filename == SOURCE_LANG+suffix):
            continue
        if suffix: # remove provided suffix
            filename = filename[0:-len(suffix)]
        filepath = os.path.join(LOCALE_DIR, filename)
        yield(filename, filepath)

FIX_RE = re.compile(b'[\x00-\x09\x0b\x0c\x0e-\x1f]')
def remove_invalid_characters(s):
    '''Remove invalid characters from translation string'''
    return FIX_RE.sub(b'', s)

# Override cdata escape function to make our output match Qt's (optional, just for cleaner diffs for
# comparison, disable by default)
_orig_escape_cdata = None
def escape_cdata(text):
    text = _orig_escape_cdata(text)
    text = text.replace("'", '&apos;')
    text = text.replace('"', '&quot;')
    return text

def postprocess_translations(reduce_diff_hacks=False):
    print('Checking and postprocessing...')

    if reduce_diff_hacks:
        global _orig_escape_cdata
        _orig_escape_cdata = ET._escape_cdata
        ET._escape_cdata = escape_cdata

    for (filename,filepath) in all_ts_files():
        os.rename(filepath, filepath+'.orig')

    have_errors = False
    for (filename,filepath) in all_ts_files('.orig'):
        # pre-fixups to cope with transifex output
        parser = ET.XMLParser(encoding='utf-8') # need to override encoding because 'utf8' is not understood only 'utf-8'
        with open(filepath + '.orig', 'rb') as f:
            data = f.read()
        # remove control characters; this must be done over the entire file otherwise the XML parser will fail
        data = remove_invalid_characters(data)
        tree = ET.parse(io.BytesIO(data), parser=parser)

        # iterate over all messages in file
        root = tree.getroot()
        for context in root.findall('context'):
            for message in context.findall('message'):
                numerus = message.get('numerus') == 'yes'
                source = message.find('source').text
                translation_node = message.find('translation')
                # pick all numerusforms
                if numerus:
                    translations = [i.text for i in translation_node.findall('numerusform')]
                else:
                    translations = [translation_node.text]

                for translation in translations:
                    if translation is None:
                        continue
                    errors = []
                    valid = check_format_specifiers(source, translation, errors, numerus)

                    for error in errors:
                        print('%s: %s' % (filename, error))

                    if not valid: # set type to unfinished and clear string if invalid
                        translation_node.clear()
                        translation_node.set('type', 'unfinished')
                        have_errors = True

                # Remove location tags
                for location in message.findall('location'):
                    message.remove(location)

                # Remove entire message if it is an unfinished translation
                if translation_node.get('type') == 'unfinished':
                    context.remove(message)

        # check if document is (virtually) empty, and remove it if so
        num_messages = 0
        for context in root.findall('context'):
            for message in context.findall('message'):
                num_messages += 1
        if num_messages < MIN_NUM_MESSAGES:
            print('Removing %s, as it contains only %i messages' % (filepath, num_messages))
            continue

        # write fixed-up tree
        # if diff reduction requested, replace some XML to 'sanitize' to qt formatting
        if reduce_diff_hacks:
            out = io.BytesIO()
            tree.write(out, encoding='utf-8')
            out = out.getvalue()
            out = out.replace(b' />', b'/>')
            with open(filepath, 'wb') as f:
                f.write(out)
        else:
            tree.write(filepath, encoding='utf-8')
    return have_errors

def update_git():
    '''
    Add new files to git repository.
    (Removing files isn't necessary here, as `git commit -a` will take care of removing files that are gone)
    '''
    file_paths = [filepath for (filename, filepath) in all_ts_files()]
    subprocess.check_call([GIT, 'add'] + file_paths)


def update_build_systems():
    '''
    Update build system and Qt resource descriptors.
    '''
    filename_lang = [re.match(r'((agenor_(.*)).ts)$', filename).groups() for (filename, filepath) in all_ts_files(include_source=True)]
    filename_lang.sort(key=lambda x: x[0])

    # update qrc locales
    with open('src/qt/agenor_locale.qrc', 'w') as f:
        f.write('<!DOCTYPE RCC><RCC version="1.0">\n')
        f.write('    <qresource prefix="/translations">\n')
        for (filename, basename, lang) in filename_lang:
            f.write(f'        <file alias="{lang}">locale/{basename}.qm</file>\n')
        f.write('    </qresource>\n')
        f.write('</RCC>\n')

    # update Makefile include
    with open('src/Makefile.qt_locale.include', 'w') as f:
        f.write('QT_TS = \\\n')
        f.write(' \\\n'.join(f'  qt/locale/{filename}' for (filename, basename, lang) in filename_lang))
        f.write('\n') # make sure last line doesn't end with a backslash


if __name__ == '__main__':
    parser = argparse.ArgumentParser(add_help=False,
                                     usage='%(prog)s [update-translations.py options] [flags]',
                                     description=__doc__,
                                     epilog='',
                                     formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('--ignore_completion', '-i', action='store_true', help='fetch all translations, even those not reaching the completion threshold')
    args, unknown_args = parser.parse_known_args()

    check_at_repository_root()
    remove_current_translations()
    fetch_all_translations(args.ignore_completion)
    postprocess_translations()
    update_git()
    update_build_systems()

