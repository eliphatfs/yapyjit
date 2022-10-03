import argparse
import importlib
from . import __name__ as rel_name


def main(args):
    importlib.import_module(f'.scripts.{args.tool}', rel_name)


if __name__ == '__main__':
    args = argparse.ArgumentParser('yapyjit_tools', description='Tools for yapyjit.')
    args.add_argument('tool')
    main(args.parse_args())
