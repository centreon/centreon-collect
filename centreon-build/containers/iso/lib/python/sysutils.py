# coding: utf-8

import subprocess

class ExecError(Exception): pass


def runcmd(cmd, verbose=False, **kwargs):
    if not verbose:
        kwargs['stdout'] = subprocess.PIPE
        kwargs['stderr'] = subprocess.STDOUT
    p = subprocess.Popen(
        cmd, shell=True, **kwargs
    )
    if not verbose:
        output = p.communicate()[0]
        return p.returncode, output
    p.wait()
    return p.returncode, None


def runcmd_and_check(cmd, **kwargs):
    code, out = runcmd(cmd, **kwargs)
    print out
    if code:
        raise ExecError(out)


def __prepare_msg(msg):
    if type(msg) is unicode:
        return msg.encode("utf-8")
    return ("%s" % msg).decode("utf-8")


def print_info(msg):
    print '\033[1;34m%s\033[1;m' % __prepare_msg(msg)


def print_success(msg):
    print '\033[1;32m%s\033[1;m' % __prepare_msg(msg)


def print_error(msg):
    print '\033[1;31m%s\033[1;m' % __prepare_msg(msg)


def print_warning(msg):
    print '\033[1;33m%s\033[1;m' % __prepare_msg(msg)
