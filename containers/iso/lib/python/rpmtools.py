# coding: utf-8
import os
import ConfigParser
import sysutils


def build_package(pkgpath, extra_dependencies=None, 
                  extra_files=None, name_suffix=None):
    config = ConfigParser.SafeConfigParser()
    try:
        fp = open(os.path.abspath(os.path.join(pkgpath, 'pkg.config')))
        config.readfp(fp)
    except IOError:
        sysutils.print_warning(u"RPM %s ignoré : fichier 'pkg.config' introuvable" % pkgpath)
        return
    if not os.path.exists("build"):
        os.mkdir("build")
    name = config.get("package", "name")
    if name_suffix is not None:
        name += "-%s" % name_suffix
    version = config.get("package", "version")
    release = config.get("package", "release")
    arch = config.get("package", "arch", vars={"arch" : "noarch"})
    output_file = "build/%s-%s-%s.%s.rpm" % (name, version, release, arch)
    extraopts = []
    for script in ["preinstall", "install", "desinstall", "changelog"]:
        fpath = os.path.join(pkgpath, script)
        if os.path.exists(fpath):
            extraopts.append("--%s %s" % (script, fpath))
    dependencies = []
    if config.has_option("package", "dependencies"):
        dependencies += config.get("package", "dependencies").split()
    if extra_dependencies is not None:
        dependencies += extra_dependencies
    if dependencies:
        extraopts += ["--dependance-rpm %s" % d for d in dependencies]
    sysutils.print_info("Construction du RPM %s" % name)
    cmd = "bin/make-rpm -o %s --version %s --release %s --nom %s --description %s %s " \
        % (output_file, version, release, name, os.path.join(pkgpath, "description"), 
           " ".join(extraopts),)

    files = config.get("package", "files")
    files = files.split()
    if extra_files is not None:
        files += extra_files
    newfiles = []
    for f in files:
        dst, src = f.split("=")
        attrs = None
        if "," in src:
            src, attrs = src.split(",", 1)
        src = os.path.realpath(os.path.join(pkgpath, src))
        newfiles.append("%s=%s%s" % (dst, src, ",%s" % attrs if attrs else ""))
    cmd += " ".join(newfiles)
    sysutils.runcmd_and_check(cmd)
    return output_file


def sign_package(pkgpath, timeout=5):
    import pexpect

    keyname = "8A7652BC"
    macrosfile = "%s/.rpmmacros" % os.environ["HOME"]
    if not os.path.exists(macrosfile):
        sysutils.print_warning("Initialisation des macros RPM dans %s" % macrosfile)
        with open(macrosfile, "w") as fp:
            fp.write("""signature gpg
%%_gpg_name %s
""" % keyname)

    code, out = sysutils.runcmd("gpg --list-key %s" % keyname)
    if code == 2:
        sysutils.print_warning(u"Import de la clé GPG dans le magasin local")
        sysutils.runcmd_and_check("gpg --import gpg/ces-*-key.gpg")
        sysutils.runcmd_and_check("rpm --import gpg/ces-public-key.gpg")
    sysutils.print_info("Signature du fichier RPM %s" % pkgpath)
    os.environ['LANG'] = 'C'
    child = pexpect.spawn("rpm --resign %s" % pkgpath)
    child.expect("Enter pass phrase:", timeout=timeout)
    child.sendline('Az3p0Tuk!7')
    child.expect("Pass phrase is good", timeout=timeout)
