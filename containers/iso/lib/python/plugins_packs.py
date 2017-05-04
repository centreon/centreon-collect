# coding: utf-8
import os
import tempfile
import shutil
import ConfigParser
#from lxml import etree
import re
import sysutils
import rpmtools


class PluginsPackError(Exception):
    pass


class PluginsPackProject(object):
    """Projet (au sens redmine) associé à un plugins pack

    """
    default_trackers = [1, 2, 10]
    developer_role = 4
    reporter_role = 5
    gitbot_user = 63
    ces_packs_reporters = 65
    ces_packs_developers = 66

    def __init__(self, projid, rclt, cmdline_args,
                 path_to_config="plugins-packs", name=None, **kwargs):
        self.projid = projid
        self.ppname = projid.replace('plugins-packs-', '')
        self.repopath = None
        self.path_to_config = path_to_config
        self.rclt = rclt
        self.args = cmdline_args
        self.clean_workdir = True
        self.__project = rclt.project(projid)
        self.data_dir = "/usr/share/centreon/packs/%s" % self.ppname
        self.docs_dir = "/usr/share/doc/centreon-packs/%s" % self.ppname
        if self.__project is None:
            self.__project = rclt.add_project(
                name, projid, scm="Git",
                tracker_ids=self.default_trackers,
                **kwargs
            )
            if not self.__project or self.__project is None:
                raise PluginsPackError("Impossible de créer le projet '%s'", projid)
            self.apply_permissions()

    def __del__(self):
        if self.clean_workdir and hasattr(self, "workdir"):
            shutil.rmtree(self.workdir)

    def __str__(self):
        return self.projid

    def apply_permissions(self):
        self.rclt.add_project_member(
            self.projid, self.gitbot_user, [self.developer_role])
        self.rclt.add_project_member(
            self.projid, self.ces_packs_developers, [self.developer_role])
        self.rclt.add_project_member(
            self.projid, self.ces_packs_reporters, [self.reporter_role])

    def init_repository(self, old_repo_path=None):
        self.clone_repository()
        try:
            dirs = ["src", "doc", "templates", "pkg"]
            for dname in dirs:
                dirpath = os.path.join(self.repopath, dname)
                os.mkdir(os.path.join(self.repopath, dname))
                fp = open(os.path.join(dirpath, "delete-me"), "w")
                fp.close()
            cmd = "git add %s; git commit -m 'Default layout'" % " ".join(dirs)
            sysutils.runcmd_and_check(cmd, cwd=self.repopath)

            if old_repo_path is not None:
                plugins_path = os.path.join(old_repo_path, "plugins")
                if os.path.exists(plugins_path):
                    sysutils.print_warning("  Copie des plugins")
                    code, output = sysutils.runcmd(
                        "cp -f %s/* %s/src" % (plugins_path, self.repopath),
                        cwd=self.repopath
                    )
                    if not code:
                        sysutils.runcmd_and_check(
                            "git add src; git commit -a -m 'Old plugins import'",
                            cwd=self.repopath
                        )
        except OSError, e:
            raise sysutils.ExecError("Impossible d'initialiser le dépôt\n%s" % str(e))

        self.add_doc_template(push=False)
        self.add_pkg_templates(push=False)
        self.update_package(push=False)
        self.add_git_ignore_file()
        self.push_to_origin()

    def clone_repository(self):
        if self.repopath is None:
            if self.args.workdir is not None:
                self.workdir = self.args.workdir
                if not os.path.exists(self.workdir):
                    os.mkdir(self.workdir)
                self.clean_workdir = False
            else:
                self.workdir = tempfile.mkdtemp()
            cmd = "git clone %(url)s/%(project)s.git" \
                % {"url": "http://gitbot:gitbot@git.int.centreon.com",
                   "project": self.projid}
            sysutils.runcmd_and_check(cmd, cwd=self.workdir)
            self.repopath = os.path.join(self.workdir, self.projid)

    def get_plugins(self):
        self.clone_repository()
        path = os.path.join(self.repopath, "src")
        for p in os.listdir(path):
            if p in [".", "..", "delete-me"]:
                continue
            yield p

    def push_to_origin(self):
        if self.repopath is None:
            return
        sysutils.runcmd_and_check(
            "git push origin master",
            cwd=self.repopath
        )

    def add_doc_template(self, push=True):
        if self.path_to_config is None:
            raise PluginsPackError(u"Modèle de documentation indisponible")
        self.clone_repository()
        if os.path.exists("%s/doc/conf.py" % self.repopath):
            sysutils.print_warning(u"Le modèle existe déjà")
            return
        cmd = "cp -r %s/doc/* %s/doc/" % (self.path_to_config, self.repopath)
        sysutils.runcmd_and_check(cmd)
        sysutils.runcmd_and_check(
            "git add doc; git commit -a -m 'Documentation template'",
            cwd=self.repopath
        )
        if push:
            self.push_to_origin()

    def add_pkg_templates(self, push=True):
        if self.path_to_config is None:
            raise PluginsPackError(u"Modèles de packages indisponibles")
        self.clone_repository()
        if os.path.exists("%s/pkg/plugins" % self.repopath):
            sysutils.print_warning(u"Le modèle existe déjà")
            return
        cmd = "cp -r %s/pkg %s" % (self.path_to_config, self.repopath)
        sysutils.runcmd_and_check(cmd)
        sysutils.runcmd_and_check(
            "git add pkg; git commit -a -m 'Packages templates'",
            cwd=self.repopath
        )
        if push:
            self.push_to_origin()

    def add_git_ignore_file(self):
        self.clone_repository()
        fp = open("%s/.gitignore" % self.repopath, "w")
        print >> fp, "doc/_build"
        fp.close()
        sysutils.runcmd_and_check(
            "git add .gitignore; git commit -a -m 'gitignore file'",
            cwd=self.repopath
        )

    def __find_dependencies(self):
        f = "%s/templates/plugins-pack.xml" % self.repopath
        if not os.path.exists(f):
            raise PluginsPackError("Fichier XML introuvable")
        # Patched by QGA
        #expr = re.compile(r'\$USER1\$/([^\s]+)')
        #tree = etree.parse(f)
        #rl = tree.xpath("//command_line/text()")
        dependencies = []
        #for r in rl:
        #    match = expr.match(r)
        #    if match is None:
        #        continue
        #    dependencies.append("/usr/lib/nagios/plugins/%s" % match.group(1))
        result = []
        for d in dependencies:
            if not d in result:
                result.append(d)
        return result

    def __build_package(self, pkg, **kwargs):
        pkg_path = os.path.join(self.repopath, "pkg/%s" % pkg)
        rpmfile = rpmtools.build_package(pkg_path, **kwargs)
        if rpmfile is not None:
            rpmtools.sign_package(rpmfile)
            sysutils.print_success("OK")

    def __include_html_doc(self, basedir, rel_path, files):
        """Inclusion récursive de la documentation HTML

        :param string basedir: emplacement de départ pour inclure la doc
        :param string rel_path: emplacement relatif à basedir dans
                                lequel effectuer la recherche
        :param list files: liste contenant le résultat
        """
        for f in os.listdir(os.path.join(basedir, rel_path)):
            if f in ['.', '..']:
                continue
            if os.path.isdir(os.path.join(basedir, rel_path, f)):
                self.__include_html_doc(basedir, os.path.join(rel_path, f), files)
                continue
            files += ["%(installdir)s/html/%(file)s=../../doc/_build/html/%(file)s" % {
                "installdir": self.docs_dir,
                "file": os.path.join(rel_path, f)
            }]

    def build_packages(self):
        """Construction des paquetages"""
        self.clone_repository()
        config, path = self.__open_pkg_config("plugins")
        files = config.get("package", "files")
        if files.strip() != "":
            self.__build_package("plugins")
        dependencies = ["centreon-import", "plugins-packs-doc", "'centreon-pp-manager >= 1.1.0'"] + self.__find_dependencies()
        extra_files = []
        if self.args.with_html_doc:
            sysutils.print_info(u"Génération de la documentation HTML")
#            os.makedirs("%s/doc/centreon/static" % self.repopath)
#            with open("%s/doc/centreon/theme.conf" % self.repopath, "w") as fp:
#                fp.write("""[theme]
#inherit = default
#stylesheet = centreon.css
#pygments_style = tango
#""")
#            with open("%s/doc/centreon/layout.html" % self.repopath, "w") as fp:
#                fp.write("""{% extends "basic/layout.html" %}
#{% block relbar1 %}
#<div id="navigation-bar"></div>
#<div class="relbar1">
#  {{ super() }}
#</div>
#{% endblock %}
#""")
#            src_css = "http://documentation.centreon.com/docs/centreon/en/latest/_static/centreon.css"
#            dst_css = "%s/doc/centreon/static/centreon.css_t" % self.repopath
#            sysutils.runcmd_and_check(
#                "wget %s -O %s" % (src_css, dst_css)
#            )
            shutil.copytree('styles/sphinx-centreon', "%s/doc/centreon" % self.repopath)

            sysutils.runcmd_and_check(
                'make html SPHINXOPTS="-Dhtml_theme=centreon -Dhtml_theme_path=[\'.\']"',
                cwd="%s/doc" % self.repopath
            )
            docfiles = []
            self.__include_html_doc(
                "%s/doc/_build/html" % self.repopath, "", docfiles
            )
            extra_files += docfiles
        if self.args.with_pdf_doc:
            sysutils.print_info(u"Génération de la documentation PDF")
            sysutils.runcmd_and_check(
                "make latexpdf", cwd="%s/doc" % self.repopath
            )
            extra_files.append(
                "%(installdir)s/pdf/%(docname)s=../../doc/_build/latex/PluginsPack.pdf" \
                    % {"installdir": self.docs_dir, "docname": "%s.pdf" % self.projid}
            )

        self.__build_package("pack", extra_dependencies=dependencies,
                             extra_files=extra_files)

    def __open_pkg_config(self, pkgtype, parser="Safe"):
        """Ouverture d'un fichier de configuration pour paquetage

        Utilisation du module standard ConfigParser.

        :param pkgtype: le type du paquetage (plugins ou pack)
        :return: un couple (objet ``ConfigParser``, chemin d'accès du
                 fichier de conf.)
        """
        config = getattr(ConfigParser, "%sConfigParser" % parser)()
        cfgpath = "%s/pkg/%s/pkg.config" % (self.repopath, pkgtype)
        config.read(cfgpath)
        return config, cfgpath

    def __save_pkg_config(self, pkgtype, prefix, display_name):
        config, cfgpath = self.__open_pkg_config(pkgtype)
        name = config.get("package", "name")
        if name.endswith("FIXME"):
            name = "%s-%s" % (prefix, display_name)
            config.set("package", "name", name)
            config.write(open(cfgpath, "w"))

    def __write_description(self, pkgtype, template, **tplargs):
        fp = open("%s/pkg/%s/description" % (self.repopath, pkgtype), "w")
        print >> fp, template % tplargs
        fp.close()

    def __guess_plugin_deps(self, plugin):
        code, output = sysutils.runcmd("file %s" % plugin)
        if code:
            raise PluginsPackError(
                u"Impossible de trouver les dépendances pour %s" % plugin
            )
        match = re.search(
            r'(python|perl)', output.strip(), flags=re.IGNORECASE
        )
        if match is None:
            return None
        return match.group(1).lower()

    def update_package(self, push=True):
        sysutils.print_info(
            u"Mise à jour des paquetages pour %s" % self.projid
        )
        self.clone_repository()
        display_name = '-'.join([w.capitalize() for w in self.ppname.split('-')])
        self.__save_pkg_config("plugins", "ces-plugins", display_name)
        self.__write_description("plugins", "A set of plugins dedicated to %(name)s monitoring.",
                                 name=display_name)
        self.__save_pkg_config("pack", "ces-pack", display_name)
        self.__write_description("pack", "Ready to use configuration templates to monitor %(name)s.",
                                 name=display_name)

        fp = open("%s/pkg/pack/install" % self.repopath, "w")
        print >> fp, "/usr/bin/php /usr/share/centreon/bin/importTemplate.php -f %s/template.xml" % self.data_dir
        fp.close()

        sysutils.runcmd_and_check(
            "git add pkg; git commit -a -m 'Updating packages'",
            cwd=self.repopath
        )
        if push:
            self.push_to_origin()

    def add_plugins_to_package(self):
        """Inclusion des plugins dans le paquetage approprié

        Tous les plugins contenus dans *src/* sont automatiquement
        ajouté dans le fichier de configuration du paquetage *plugins*.
        """
        sysutils.print_info(u"Inclusion des plugins dans le paquetage pour %s" % self.projid)
        self.clone_repository()
        plugins_dir = "%s/src" % self.repopath
        config, cfgpath = self.__open_pkg_config("plugins")
        plist = []
        dependencies = []
        for f in os.listdir(plugins_dir):
            if f in ['.', '..', 'delete-me']:
                continue
            plist.append(f)
        pattern = "/usr/lib/nagios/plugins/%(ppname)s/%(plugin)s=../../src/%(plugin)s,centreon:centreon,0755"
        pkgfiles = []
        for p in plist:
            dep = self.__guess_plugin_deps("%s/%s" % (plugins_dir, p))
            if dep is not None and not dep in dependencies:
                dependencies.append(dep)
            pkgfiles.append(pattern % {"ppname": self.ppname, "plugin": p})
        if dependencies:
            config.set("package", "dependencies", " ".join(dependencies))
        config.set("package", "files", "\n".join(pkgfiles))
        config.write(open(cfgpath, "w"))

        sysutils.runcmd_and_check(
            "git add pkg/plugins; git commit -a -m 'Adding files to package'",
            cwd=self.repopath
        )
        self.push_to_origin()

    def add_agent_data_to_package(self):
        """Ajout des fichiers nécessaires pour la supervision via agent

        Si un *plugins pack* déclare un répertoire ``agent_data`` à la
        racine de son dépôt, tous les fichiers contenus dedans seront
        ajoutés au paquetage *pack*.
        """
        sysutils.print_info(u"Inclusion des données de l'agent pour %s" % self.projid)
        self.clone_repository()
        data_dir = "%s/agent_data" % self.repopath
        if not os.path.exists(data_dir):
            return
        config, cfgpath = self.__open_pkg_config("pack", parser="Raw")
        files = config.get("package", "files")
        files = files.split()
        origlen = len(files)
        pattern = "%(installdir)s/agent/%(file)s=../../agent_data/%(file)s,centreon:centreon,0444"
        for f in os.listdir(data_dir):
            if f in ['.', '..', 'delete-me']:
                continue
            newfile = pattern % {"installdir": self.data_dir, "file": f}
            if not newfile in files:
                sysutils.print_warning(u"  Ajout du fichier %s" % f)
                files.append(newfile)
        if len(files) == origlen:
            return
        config.set("package", "files", "\n".join(files))
        config.write(open(cfgpath, "w"))
        sysutils.runcmd_and_check(
            "git add pkg/pack; git commit -a -m 'Agent data added to package.'",
            cwd=self.repopath
        )
        self.push_to_origin()

    def rename_plugins_package(self):
        sysutils.print_info(u"Renommage du paquetage *plugins* en *ces-plugins* pour %s" % self.projid)
        self.clone_repository()
        config, cfgpath = self.__open_pkg_config("plugins")
        name = config.get("package", "name")
        name = "ces-%s" % name
        config.set("package", "name", name)
        config.write(open(cfgpath, "w"))
        sysutils.runcmd_and_check(
            "git add pkg/plugins; git commit -a -m 'Renaming package *plugins* to *ces-plugins*'",
            cwd=self.repopath
        )
        self.push_to_origin()

    def rename_postinstall(self):
        sysutils.print_info(u"Renommage du fichier *postinstall* en *install* pour %s" % self.projid)
        self.clone_repository()
        for pkgtype in ["pack", "plugins"]:
            path = os.path.join(self.repopath, "pkg/%s/postinstall" % pkgtype)
            if os.path.exists(path):
                sysutils.runcmd_and_check(
                    "git mv --force pkg/%(pkgtype)s/postinstall pkg/%(pkgtype)s/install" % {"pkgtype": pkgtype},
                    cwd=self.repopath
                )
        sysutils.runcmd_and_check(
            "git add pkg; git commit -a -m 'Renaming *postinstall* files to *install*'",
            cwd=self.repopath
        )
        self.push_to_origin()

    def change_tpl_path(self):
        sysutils.print_info(u"Déplacement de template.xml pour %s" % self.projid)
        self.clone_repository()
        config, cfgpath = self.__open_pkg_config("pack")
        files = config.get("package", "files")
        files = files.split()
        if len(files) > 1:
            sysutils.print_warning("  Traitement manuel requis pour %s" % self.projid)
            return
        config.set("package", "files", "%s/template.xml=../../templates/plugins-pack.xml,centreon:centreon" % self.data_dir)
        config.write(open(cfgpath, "w"))
        sysutils.runcmd_and_check(
            "git add pkg/pack; git commit -a -m 'Changing *template.xml* installation path'",
            cwd=self.repopath
        )
        self.push_to_origin()
