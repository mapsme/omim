from __future__ import print_function

import logging
import subprocess

from integration_tests_runner import setup_jenkins_console_logger


FILE_EXTENSIONS = [".cpp", ".hpp"]
EXCLUDE_FOLDERS = ["base"]

setup_jenkins_console_logger()

def git(command):
    return exec_shell("git", command).strip()

def clang_format(source_file):
    return exec_shell("clang-format", "-i {0}".format(source_file))

def exec_shell(command, flags):
    spell = "{0} {1}".format(command, flags)

    process = subprocess.Popen(
        [spell],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        shell=True
    )

    logging.getLogger().info(spell)
    out, _ = process.communicate()
    return out


def current_commit():
    return git("rev-parse HEAD")


def main_parent_of(sha):
    return git("show --format='%H' {0}^1".format(sha))


def files_in_diff():
    cur_commit = current_commit()
    main_parent = main_parent_of(cur_commit)
    diff_files = git(
        "diff --name-only {cur} {parent}".format(
            cur=cur_commit, parent=main_parent
        )
    )

    return filter(filter_files, diff_files)


def filter_files(file_name):
    filtered_extensions = filter(lambda s: file_name.endswith(s), FILE_EXTENSIONS)
    if not filtered_extensions:
        return False

    filtered_folders = filter(lambda s: file_name.startswith(s), EXCLUDE_FOLDERS)
    if filtered_folders:
        return False

    return True


def format_files(files):
    if files:
        files_to_format = " ".join(files)
        clang_format(files_to_format)


def git_diff():
    return git("diff")


def main():
    print("I am main")
    cur_com = current_commit()
    print("Current commit: {}".format(cur_com))
    main_parent = main_parent_of(cur_com)
    print("Main parent is: {}".format(main_parent))

    files_to_format = files_in_diff()
    format_files(files_to_format)

    diff = git_diff()

    if diff:
        print("We have a diff, so the check has failed")
        logging.getLogger().info(diff)
        exit(2)









if __name__ == '__main__':
    main()


