#!/usr/bin/env python3

import core_option_translation as t


if __name__ == '__main__':
    try:
        if t.os.path.isfile(t.sys.argv[1]):
            _temp = t.os.path.dirname(t.sys.argv[1])
        else:
            _temp = t.sys.argv[1]
        while _temp.endswith('/') or _temp.endswith('\\'):
            _temp = _temp[:-1]
        TARGET_DIR_PATH = _temp
    except IndexError:
        TARGET_DIR_PATH = t.os.path.dirname(t.os.path.dirname(t.os.path.realpath(__file__)))
        print("No path provided, assuming parent directory:\n" + TARGET_DIR_PATH)

    NAME = 'core_options'
    DIR_PATH = t.os.path.dirname(t.os.path.realpath(__file__))
    US_FILE_PATH = t.os.path.join(DIR_PATH, '_us', f'{NAME}.h')
    H_FILE_PATH = t.os.path.join(TARGET_DIR_PATH, 'libretro_core_options.h')
    INTL_FILE_PATH = t.os.path.join(TARGET_DIR_PATH, 'libretro_core_options_intl.h')

    print('Getting texts from libretro_core_options.h')
    with open(H_FILE_PATH, 'r+', encoding='utf-8') as _h_file:
        _main_text = _h_file.read()
    _hash_n_str = t.get_texts(_main_text)
    _files = t.create_msg_hash(DIR_PATH, NAME, _hash_n_str)

    print('Converting translations *.json to *.h:')
    for _folder in t.os.scandir(DIR_PATH):
        if _folder.is_dir() and _folder.name.startswith('_') and _folder.name != '__pycache__':
            print(_folder.name)
            t.json2h(DIR_PATH, _folder.path, NAME)

    print('Constructing libretro_core_options_intl.h')
    t.create_intl_file(INTL_FILE_PATH, DIR_PATH, _main_text, NAME, _files["_us"])

    print('\nAll done!')
