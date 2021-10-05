%define with_python %{?_without_python: 0} %{?!_without_python: 1}
%define with_php %{?_without_php: 0} %{?!_without_php: 0}
%define with_tcl %{?_without_tcl: 0} %{?!_without_tcl: 1}
%define with_ruby %{?_without_ruby: 0} %{?!_without_ruby: 1}
%define with_lua %{?_without_lua: 0} %{?!_without_lua: 1}
%define with_systemd %{?_without_systemd: 0} %{?!_without_systemd: 1}
%define php_extdir %(php-config --extension-dir 2>/dev/null || echo %{_libdir}/php4)
%define svnrev r1190
#define pretag 1.2.99908020600

# Private libraries are not be exposed globally by RPM
# RPM 4.9
%global __provides_exclude_from %{?__provides_exclude_from:%__provides_exclude_from|}%{php_extdir}/.*\\.so$


Summary: Round Robin Database Tool to store and display time-series data
Name: rrdtool
Version: 1.7.2
Release: 1%{?dist}
License: GPLv2+ with exceptions
Group: Applications/Databases
URL: http://oss.oetiker.ch/rrdtool/
Source0: http://oss.oetiker.ch/%{name}/pub/%{name}-%{version}.tar.gz
Source1: php4-%{svnrev}.tar.gz

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Requires: dejavu-sans-mono-fonts
BuildRequires: gcc-c++, openssl-devel, freetype-devel
BuildRequires: libpng-devel, zlib-devel, intltool >= 0.35.0
BuildRequires: cairo-devel >= 1.4.6, pango-devel >= 1.17
BuildRequires: libtool, groff
BuildRequires: gettext, libxml2-devel
BuildRequires: perl-ExtUtils-MakeMaker, perl-devel, automake, autoconf

%description
RRD is the Acronym for Round Robin Database. RRD is a system to store and
display time-series data (i.e. network bandwidth, machine-room temperature,
server load average). It stores the data in a very compact way that will not
expand over time, and it presents useful graphs by processing the data to
enforce a certain data density. It can be used either via simple wrapper
scripts (from shell or Perl) or via frontends that poll network devices and
put a friendly user interface on it.

%package devel
Summary: RRDtool libraries and header files
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}
Requires: pkgconfig

%description devel
RRD is the Acronym for Round Robin Database. RRD is a system to store and
display time-series data (i.e. network bandwidth, machine-room temperature,
server load average). This package allow you to use directly this library.

%package doc
Summary: RRDtool documentation
Group: Documentation

%description doc
RRD is the Acronym for Round Robin Database. RRD is a system to store and
display time-series data (i.e. network bandwidth, machine-room temperature,
server load average). This package contains documentation on using RRD.

%package perl
Summary: Perl RRDtool bindings
Group: Development/Languages
Requires: %{name} = %{version}-%{release}
Requires: perl(:MODULE_COMPAT_%(eval "`%{__perl} -V:version`"; echo $version))
Obsoletes: perl-%{name} < %{version}-%{release}
Provides: perl-%{name} = %{version}-%{release}

%description perl
The Perl RRDtool bindings

%if %{with_python}
# Make sure the runtime python is newer than the build one;
# give a default value to handle parsing in cases when python is not present:
%{!?rrd_python_version: %define rrd_python_version %(%{__python} -c 'import sys; print sys.version.split(" ")[0]' || echo "3.14")}

%package python
Summary: Python RRDtool bindings
Group: Development/Languages
BuildRequires: python-devel
Requires: python >= %{rrd_python_version}
Requires: %{name} = %{version}-%{release}
Obsoletes: python-%{name} < %{version}-%{release}
Provides: python-%{name} = %{version}-%{release}

%description python
Python RRDtool bindings.
%endif

%ifarch %{power64}
# php bits busted on ppc64 at the moment
%define with_php 0
%endif

%if %{with_php}
%package php
Summary: PHP RRDtool bindings
Group: Development/Languages
BuildRequires: php-devel >= 4.0
Requires: php >= 4.0
Requires: %{name} = %{version}-%{release}
Requires: php(zend-abi) = %{php_zend_api}
Requires: php(api) = %{php_core_api}
Obsoletes: php-%{name} < %{version}-%{release}
Provides: php-%{name} = %{version}-%{release}
Provides: php-pecl(rrdtool)

%description php
The %{name}-php package includes a dynamic shared object (DSO) that adds
RRDtool bindings to the PHP HTML-embedded scripting language.
%endif

%if %{with_tcl}
%package tcl
Summary: Tcl RRDtool bindings
Group: Development/Languages
BuildRequires: tcl-devel >= 8.0
Requires: tcl >= 8.0
Requires: %{name} = %{version}-%{release}
Obsoletes: tcl-%{name} < %{version}-%{release}
Provides: tcl-%{name} = %{version}-%{release}

%description tcl
The %{name}-tcl package includes RRDtool bindings for Tcl.
%endif

%if %{with_ruby}
%define ruby_sitearch %(/usr/bin/ruby -rrbconfig -e "puts RbConfig::CONFIG['sitearchdir']")

%package ruby
Summary: Ruby RRDtool bindings
Group: Development/Languages
BuildRequires: ruby, ruby-devel
Requires: %{name} = %{version}-%{release}

%description ruby
The %{name}-ruby package includes RRDtool bindings for Ruby.
%endif

%if %{with_lua}
%define luaver 5.1
%define lualibdir %{_libdir}/lua/%{luaver}
%define luapkgdir %{_datadir}/lua/%{luaver}

%package lua
Summary: Lua RRDtool bindings
Group: Development/Languages
BuildRequires: lua, lua-devel
Requires: lua(abi) = %{luaver}
Requires: %{name} = %{version}-%{release}

%description lua
The %{name}-lua package includes RRDtool bindings for Lua.
%endif

%package cached
Summary: Data caching daemon for RRDtool
Group: Applications/Databases
Requires: %{name} = %{version}-%{release}
#%if %{with_systemd}
#BuildRequires: with_systemd
#%endif

%description cached
rrdcached is a daemon that receives updates to existing RRD files,
accumulates them and, if enough have been received or a defined time has
passed, writes the updates to the RRD file.  The daemon was written with
big setups in mind which usually runs into I/O related problems.  This
daemon was written to alleviate these problems.

%prep
%setup -q -n %{name}-%{version} %{?with_php: -a 1}
%if %{with_php}
#%patch1 -p1 -b .php54
#%patch3 -p1 -b .php55
%endif

# Fix to find correct python dir on lib64
%{__perl} -pi -e 's|get_python_lib\(0,0,prefix|get_python_lib\(1,0,prefix|g' \
    configure

# Most edits shouldn't be necessary when using --libdir, but
# w/o, some introduce hardcoded rpaths where they shouldn't
%{__perl} -pi.orig -e 's|/lib\b|/%{_lib}|g' \
    configure Makefile.in php4/configure php4/ltconfig*

# Perl 5.10 seems to not like long version strings, hack around it
%{__perl} -pi.orig -e 's|1.299907080300|1.29990708|' \
    bindings/perl-shared/RRDs.pm bindings/perl-piped/RRDp.pm

#
# fix config files for php4 bindings
# workaround needed due to https://bugzilla.redhat.com/show_bug.cgi?id=211069
cp -p /usr/lib/rpm/config.{guess,sub} php4/

%build
%configure \
    --with-perl-options='INSTALLDIRS="vendor"' \
    --disable-rpath \
%if %{with_tcl}
    --enable-tcl-site \
    --with-tcllib=%{_libdir} \
%else
    --disable-tcl \
%endif
%if %{with_python}
    --enable-python \
%else
    --disable-python \
%endif
%if %{with_ruby}
    --enable-ruby \
%else
    --disable-ruby \
%endif
    --disable-static \
    --with-pic

# Fix another rpath issue
%{__perl} -pi.orig -e 's|-Wl,--rpath -Wl,\$rp||g' \
    bindings/perl-shared/Makefile.PL

%if %{with_ruby}
# Remove Rpath from Ruby
%{__perl} -pi.orig -e 's|-Wl,--rpath -Wl,\$\(EPREFIX\)/lib||g' \
    bindings/ruby/extconf.rb
sed -i 's| extconf.rb| extconf.rb --vendor |' bindings/Makefile
%endif

# Force RRDp bits where we want 'em, not sure yet why the
# --with-perl-options and --libdir don't take
pushd bindings/perl-piped/
%{__perl} Makefile.PL INSTALLDIRS=vendor
%{__perl} -pi.orig -e 's|/lib/perl|/%{_lib}/perl|g' Makefile
popd

#{__make} %{?_smp_mflags}
make

# Build the php module, the tmp install is required
%if %{with_php}
%define rrdtmp %{_tmppath}/%{name}-%{version}-tmpinstall
%{__make} install DESTDIR="%{rrdtmp}"
pushd php4/
%configure \
    --with-rrdtool="%{rrdtmp}%{_prefix}" \
    --disable-static
#{__make} %{?_smp_mflags}
make
popd
%{__rm} -rf %{rrdtmp}
%endif

# Fix @perl@ and @PERL@
find examples/ -type f \
    -exec %{__perl} -pi -e 's|^#! \@perl\@|#!%{__perl}|gi' {} \;
find examples/ -name "*.pl" \
    -exec %{__perl} -pi -e 's|\015||gi' {} \;

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR="$RPM_BUILD_ROOT" install

# Install the php module
%if %{with_php}
%{__install} -D -m0755 php4/modules/rrdtool.so \
    %{buildroot}%{php_extdir}/rrdtool.so
# Clean up the examples for inclusion as docs
%{__rm} -rf php4/examples/.svn
# Put the php config bit into place
%{__mkdir_p} %{buildroot}%{_sysconfdir}/php.d
%{__cat} << __EOF__ > %{buildroot}%{_sysconfdir}/php.d/rrdtool.ini
; Enable rrdtool extension module
extension=rrdtool.so
__EOF__
%endif

# Pesky RRDp.pm...
%{__mv} $RPM_BUILD_ROOT%{perl_vendorlib}/RRDp.pm $RPM_BUILD_ROOT%{perl_vendorarch}/

# Dunno why this is getting installed here...
%{__rm} -f $RPM_BUILD_ROOT%{perl_vendorlib}/leaktest.pl

# We only want .txt and .html files for the main documentation
%{__mkdir_p} doc2/html doc2/txt
%{__cp} -a doc/*.txt doc2/txt/
%{__cp} -a doc/*.html doc2/html/

# Put perl docs in perl package
%{__mkdir_p} doc3/html
%{__mv} doc2/html/RRD*.html doc3/html/

# Clean up the examples
%{__rm} -f examples/Makefile* examples/*.in

# This is so rpm doesn't pick up perl module dependencies automatically
find examples/ -type f -exec chmod 0644 {} \;

# Clean up the buildroot
%{__rm} -rf $RPM_BUILD_ROOT%{_docdir}/%{name}-* \
        $RPM_BUILD_ROOT%{perl_vendorarch}/ntmake.pl \
        $RPM_BUILD_ROOT%{perl_archlib}/perllocal.pod \
        $RPM_BUILD_ROOT%{_datadir}/%{name}/examples \
        $RPM_BUILD_ROOT%{perl_vendorarch}/auto/*/{.packlist,*.bs}

# Set up rrdcached
%__install -d -m 0755 $RPM_BUILD_ROOT/%{_sysconfdir}/default
%__install -d -m 0755 $RPM_BUILD_ROOT/%{_sysconfdir}/rc.d/init.d
%__install -m 0644 etc/rrdcached-default-redhat $RPM_BUILD_ROOT/%{_sysconfdir}/default/rrdcached
%__install -m 0755 etc/rrdcached-init-redhat $RPM_BUILD_ROOT/%{_sysconfdir}/rc.d/init.d/rrdcached
%__install -d -m 0755 -o nobody -g nobody $RPM_BUILD_ROOT/%{_localstatedir}/run/rrdcached

%check
# minimal load test for the PHP extension
%if %{with_php}
LD_LIBRARY_PATH=%{buildroot}%{_libdir} php -n \
    -d extension_dir=%{buildroot}%{php_extdir} \
    -d extension=rrdtool.so -m \
    | grep rrdtool
%endif

%clean
%{__rm} -rf $RPM_BUILD_ROOT

%pre cached
/usr/sbin/groupadd %rrdcached_user &>/dev/null ||:
/usr/sbin/useradd -s /sbin/nologin -g %rrdcached_user -c %rrdcached_user -d %{_localstatedir}/run/rrdcached  %rrdcached_user &>/dev/null || :

%post -p /sbin/ldconfig

%post cached
%if %{with_systemd}
    %systemd_post rrdcached.service
    %systemd_post rrdcached.socket
%else
    /sbin/chkconfig --add rrdcached || :
    if [ $1 -lt 2 ] # Installing
    then
        /sbin/service rrdcached start || :
    else # Upgrading
        /sbin/service rrdcached restart || :
    fi
%endif

%posttrans cached
%if %{with_systemd}
%else
    /sbin/chkconfig --add rrdcached || :
    /sbin/service rrdcached restart || :
%endif

%preun cached
%if %{with_systemd}
    %systemd_preun rrdcached.service
    %systemd_preun rrdcached.socket
%else
    if [ $1 -lt 1 ] # Uninstalling
    then
        /sbin/service rrdcached stop || :
    fi
%endif

%postun -p /sbin/ldconfig

%postun cached
%if %{with_systemd}
    %systemd_postun_with_restart rrdcached.service
    %systemd_postun_with_restart rrdcached.socket
%else
    if [ $1 -lt 1 ] # Uninstalling
    then
        /usr/sbin/userdel %rrdcached_user &>/dev/null || :
    fi
%endif

%files
%defattr(-,root,root,-)
%{_bindir}/*
%{_libdir}/*.so.*
%{_datadir}/%{name}
%{_mandir}/man1/*
%{_usr}/share/locale/

%files devel
%defattr(-,root,root,-)
%{_includedir}/*.h
%exclude %{_libdir}/*.la
%{_libdir}/lib*.so
%{_libdir}/pkgconfig/*.pc

%files doc
%defattr(-,root,root,-)
%doc CONTRIBUTORS COPYRIGHT TODO NEWS THREADS
%doc examples doc2/html doc2/txt

%files perl
%defattr(-,root,root,-)
%doc doc3/html
%{_mandir}/man3/*
%{perl_vendorarch}/*.pm
%attr(0755,root,root) %{perl_vendorarch}/auto/RRDs/

%if %{with_python}
%files python
%defattr(-,root,root,-)
%doc bindings/python/COPYING
%{python_sitearch}/*
%endif

%if %{with_php}
%files php
%defattr(-,root,root,0755)
%doc php4/examples php4/README
%config(noreplace) %{_sysconfdir}/php.d/rrdtool.ini
%{php_extdir}/rrdtool.so
%endif

%if %{with_tcl}
%files tcl
%defattr(-,root,root,-)
%doc bindings/tcl/README
%{_libdir}/tclrrd*.so
%{_libdir}/rrdtool/*.tcl
%endif

%if %{with_ruby}
%files ruby
%defattr(-,root,root,-)
%doc bindings/ruby/README
%{ruby_sitearch}/RRD.so
%endif

%if %{with_lua}
%files lua
%defattr(-,root,root,-)
%doc bindings/lua/README
%exclude %{lualibdir}/*.la
%{lualibdir}/*
%endif

%files cached
%{_bindir}/rrdcached
%config %{_sysconfdir}/default/*
%config %{_sysconfdir}/rc.d/init.d/*
%{_mandir}/man1/rrdcached*
%attr(0775 %rrdcached_user %rrdcached_user) %dir %{_localstatedir}/run/rrdcached
%if %{with_systemd}
%{_unitdir}/rrdcached.service
%{_unitdir}/rrdcached.socket
%endif

%changelog
* Thu Aug 14 2019 Quentin Garnier <qgarnier@centreon.com> - 1.7.1-1
- Remove php build

* Tue Aug 12 2014 Jaroslav Škarvada <jskarvad@redhat.com> - 1.4.8-9
- Fixing build on ppc64le
  Resolves: rhbz#1125670

* Fri Jan 24 2014 Daniel Mach <dmach@redhat.com> - 1.4.8-8
- Mass rebuild 2014-01-24

* Fri Dec 27 2013 Daniel Mach <dmach@redhat.com> - 1.4.8-7
- Mass rebuild 2013-12-27

* Mon Nov  4 2013 Jaroslav Škarvada <jskarvad@redhat.com> - 1.4.8-6
- Fixed multilib problems
  Resolves: rhbz#881248

* Tue Oct 15 2013 Jaroslav Škarvada <jskarvad@redhat.com> - 1.4.8-5
- Fixed twice packaging of tclrrd1.4.8.so
  Resolves: rhbz#1018169

* Fri Aug 30 2013 Jaroslav Škarvada <jskarvad@redhat.com> - 1.4.8-4
- Rebuild due to php macros change
  Resolves: rhbz#1002882

* Mon Jul  1 2013 Jaroslav Škarvada <jskarvad@redhat.com> - 1.4.8-3
- Minor doc / man fixes

* Fri Jun  7 2013 Jaroslav Škarvada <jskarvad@redhat.com> - 1.4.8-2
- Added imginfo format check
  Resolves: CVE-2013-2131

* Thu May 23 2013 Jaroslav Škarvada <jskarvad@redhat.com> - 1.4.8-1
- New version
  Resolves: rhbz#966639
- Updated and defuzzified patches
- Still use lua-5.1 (fedoras < f20)

* Mon May 20 2013 Jaroslav Škarvada <jskarvad@redhat.com> - 1.4.7-17
- Require lua abi instead of package version

* Wed May 15 2013 Tom Callaway <spot@fedoraproject.org> - 1.4.7-16
- lua 5.2

* Tue May  7 2013 Jaroslav Škarvada <jskarvad@redhat.com> - 1.4.7-15
- Removed unneccassary dejavu-lgc-sans-mono-fonts requirement
  Resolves: rhbz#922467

* Tue Mar 26 2013 Jaroslav Škarvada <jskarvad@redhat.com> - 1.4.7-14
- Fixed autoconf (by autoconf-fix patch)
- Added support for aarch64
  Resolves: rhbz#926455

* Mon Mar 25 2013 Jaroslav Škarvada <jskarvad@redhat.com> - 1.4.7-13
- Fixed build failure
  Resolves: rhbz#926037

* Fri Mar 22 2013 Remi Collet <rcollet@redhat.com> - 1.4.7-12
- rebuild for http://fedoraproject.org/wiki/Features/Php55
- remove rrdtool_logo_guid function

* Tue Mar 19 2013 Jaroslav Škarvada <jskarvad@redhat.com> - 1.4.7-11
- Dropped ruby(abi) explicit requirement

* Mon Mar 18 2013 Jaroslav Škarvada <jskarvad@redhat.com> - 1.4.7-10
- Fixed build failure with ruby-2.0 (by ruby-2-fix patch)
- Fixed bogus date in changelog

* Thu Feb 14 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.4.7-9
- Rebuilt for https://fedoraproject.org/wiki/Fedora_19_Mass_Rebuild

* Tue Jan  8 2013 Jaroslav Škarvada <jskarvad@redhat.com> - 1.4.7-8
- Removed libtool archive from the lua subpackage

* Sat Jul 21 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.4.7-7
- Rebuilt for https://fedoraproject.org/wiki/Fedora_18_Mass_Rebuild

* Mon Jun 11 2012 Petr Pisar <ppisar@redhat.com> - 1.4.7-6
- Perl 5.16 rebuild

* Thu Feb  9 2012 Jaroslav Škarvada <jskarvad@redhat.com> - 1.4.7-5
- Changed ruby(abi) to 1.9.1

* Wed Feb  8 2012 Jaroslav Škarvada <jskarvad@redhat.com> - 1.4.7-4
- Used ruby_vendorarchdir instead of ruby_sitearch

* Wed Feb  8 2012 Jaroslav Škarvada <jskarvad@redhat.com> - 1.4.7-3
- Fixed ruby(abi) requires

* Tue Feb  7 2012 Jaroslav Škarvada <jskarvad@redhat.com> - 1.4.7-2
- Rebuilt for new ruby

* Thu Jan 26 2012 Jaroslav Škarvada <jskarvad@redhat.com> - 1.4.7-1
- New version
  Resolves: rhbz#783553
- Dropped fix-tcl-site-option patch (upstreamed)

* Thu Jan 19 2012 Remi Collet <remi@fedoraproject.org> - 1.4.4-9
- build with php 5.4

* Thu Dec 29 2011 Remi Collet <remi@fedoraproject.org> - 1.4.4-8
- add patch for php 5.4
- add minimal load test for PHP extension
- add provides filters

* Tue Dec 06 2011 Adam Jackson <ajax@redhat.com> - 1.4.4-7
- Rebuild for new libpng

* Fri Jun 17 2011 Marcela Mašláňová <mmaslano@redhat.com> - 1.4.4-6
- Perl mass rebuild

* Sat Jun 11 2011 Jaroslav Škarvada <jskarvad@redhat.com> - 1.4.4-5
- Fixed build failure due to change in php_zend_api macro type

* Fri Jun 10 2011 Marcela Mašláňová <mmaslano@redhat.com> - 1.4.4-4
- Perl 5.14 mass rebuild

* Wed Feb 09 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.4.4-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Tue Nov 23 2010 Jaroslav Škarvada <jskarvad@redhat.com> - 1.4.4-2
- Fixed mixed tabs and spaces rpmlint warning
- Fixed tcl-site configure option (upstream ticket #281)
- Removed Rpath from Ruby
- Enabled Lua bindings (#656080), thanks to Tim Niemueller

* Tue Nov 16 2010 Jaroslav Škarvada <jskarvad@redhat.com> - 1.4.4-1
- Update to rrdtool 1.4.4

* Thu Jul 22 2010 David Malcolm <dmalcolm@redhat.com> - 1.3.8-8
- Rebuilt for https://fedoraproject.org/wiki/Features/Python_2.7/MassRebuild

* Wed Jun 02 2010 Marcela Maslanova <mmaslano@redhat.com> - 1.3.8-7
- Mass rebuild with perl-5.12.0

* Wed Jan 13 2010 Stepan Kasal <skasal@redhat.com> - 1.3.8-6
- remove python_* macros clashing with the built-in ones
- fix for new vendorlib directory

* Mon Dec  7 2009 Stepan Kasal <skasal@redhat.com> - 1.3.8-5
- rebuild against perl 5.10.1

* Sun Jul 26 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.3.8-4
- Rebuilt for https://fedoraproject.org/wiki/Fedora_12_Mass_Rebuild

* Mon Jul 13 2009 Remi Collet <Fedora@FamilleCollet.com> 1.3.8-3
- rebuild for new PHP 5.3.0 ABI (20090626)

* Tue May 26 2009 Jarod Wilson <jarod@redhat.com> 1.3.8-2
- Update dejavu font deps yet again, hopefully for the last time... (#473551)

* Tue May 19 2009 Jarod Wilson <jarod@redhat.com> 1.3.8-1
- Update to rrdtool 1.3.8

* Thu Apr 09 2009 Jarod Wilson <jarod@redhat.com> 1.3.7-1
- Update to rrdtool 1.3.7

* Wed Feb 25 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.3.6-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_11_Mass_Rebuild

* Mon Jan 19 2009 Jarod Wilson <jarod@redhat.com> 1.3.6-1
- Update to rrdtool 1.3.6

* Fri Jan 16 2009 Jarod Wilson <jarod@redhat.com> 1.3.5-2
- dejavu font package names changed again...

* Tue Dec 16 2008 Jarod Wilson <jarod@redhat.com> 1.3.5-1
- Update to rrdtool 1.3.5

* Thu Dec 04 2008 Ignacio Vazquez-Abrams <ivazqueznet+rpm@gmail.com> - 1.3.4-5
- Rebuild for Python 2.6

* Mon Dec 01 2008 Jarod Wilson <jarod@redhat.com> 1.3.4-4
- Update dejavu font dependencies (#473551)

* Sat Nov 29 2008 Ignacio Vazquez-Abrams <ivazqueznet+rpm@gmail.com> - 1.3.4-3
- Rebuild for Python 2.6

* Mon Oct 20 2008 Jarod Wilson <jarod@redhat.com> 1.3.4-2
- Drop php bindings patch, rrd_update changed back to its
  prior prototype post-beta (#467593)

* Mon Oct 06 2008 Jarod Wilson <jarod@redhat.com> 1.3.4-1
- Update to rrdtool 1.3.4

* Mon Sep 15 2008 Jarod Wilson <jarod@redhat.com> 1.3.3-1
- Update to rrdtool 1.3.3
  * fixes segfault on graph creation regression in 1.3.2 (#462301)

* Sat Sep 06 2008 Jarod Wilson <jwilson@redhat.com> 1.3.2-1
- Update to rrdtool 1.3.2
  * fixes a data corruption bug when rrd wraps around
  * make imginfo behave the same as docs say it does
  * fixes for numerous memory leaks

* Tue Aug 12 2008 Jarod Wilson <jwilson@redhat.com> 1.3.1-1
- Update to rrdtool 1.3.1

* Mon Jun 16 2008 Chris Ricker <kaboom@oobleck.net> 1.3.0-1
- Update to rrdtool 1.3.0

* Sun Jun 08 2008 Jarod Wilson <jwilson@redhat.com> 1.3-0.20.rc9
- Update to rrdtool 1.3 rc9
- Minor spec tweaks to permit building on older EL

* Wed Jun 04 2008 Chris Ricker <kaboom@oobleck.net> 1.3-0.19.rc7
- Update to rrdtool 1.3 rc7

* Tue May 27 2008 Chris Ricker <kaboom@oobleck.net> 1.3-0.18.rc6
- Update to rrdtool 1.3 rc6

* Wed May 21 2008 Chris Ricker <kaboom@oobleck.net> 1.3-0.17.rc4
- Bump version and rebuild

* Wed May 21 2008 Chris Ricker <kaboom@oobleck.net> 1.3-0.16.rc4
- Fix php bindings compile on x86_64

* Mon May 19 2008 Chris Ricker <kaboom@oobleck.net> 1.3-0.15.rc4
- Update to rrdtool 1.3 rc4

* Tue May 13 2008 Jarod Wilson <jwilson@redhat.com> 1.3-0.15.rc1
- Update to rrdtool 1.3 rc1
- Fix versioning in changelog entries, had an extra 0 in there...
- Drop cairo and python patches, they're in 1.3 rc1
- Add Requires: gettext and libxml2-devel for new translations

* Wed Apr 30 2008 Jarod Wilson <jwilson@redhat.com> 1.3-0.14.beta4
- Drop some conditional flags, they're not working at the moment...

* Wed Apr 30 2008 Jarod Wilson <jwilson@redhat.com> 1.3-0.13.beta4
- Fix problem with cairo_save/cairo_restore (#444827)

* Wed Apr 23 2008 Jarod Wilson <jwilson@redhat.com> 1.3-0.12.beta4
- Fix python bindings rrdtool info implementation (#435468)

* Tue Apr 08 2008 Jarod Wilson <jwilson@redhat.com> 1.3-0.11.beta4
- Work around apparent version string length issue w/perl 5.10 (#441359)

* Sat Apr 05 2008 Jarod Wilson <jwilson@redhat.com> 1.3-0.10.beta4
- Fix use of rrd_update in php bindings (#437558)

* Mon Mar  3 2008 Tom "spot" Callaway <tcallawa@redhat.com> 1.3-0.9.beta4
- rebuild for new perl (again)

* Wed Feb 13 2008 Jarod Wilson <jwilson@redhat.com> 1.3-0.8.beta4
- Update to rrdtool 1.3 beta4

* Tue Feb 05 2008 Tom "spot" Callaway <tcallawa@redhat.com> 1.3-0.7.beta3
- rebuild for new perl (and fix license tag)

* Mon Feb 04 2008 Jarod Wilson <jwilson@redhat.com> 1.3-0.6.beta3
- Plug memory leak (#430879)

* Mon Jan 07 2008 Jarod Wilson <jwilson@redhat.com> 1.3-0.5.beta3
- Fix right-aligned text alignment and scaling (Resolves: #427609)

* Wed Jan 02 2008 Jarod Wilson <jwilson@redhat.com> 1.3-0.4.beta3
- Add newly built python egg to %%files

* Wed Jan 02 2008 Jarod Wilson <jwilson@redhat.com> 1.3-0.3.beta3
- Update to rrdtool 1.3 beta3
- Return properly from errors in RRDp.pm (Resolves: #427040)
- Requires: dejavu-lgc-fonts (Resolves: #426935)

* Thu Dec 06 2007 Jarod Wilson <jwilson@redhat.com> 1.3-0.2.beta2
- Update to rrdtool 1.3 beta2

* Wed Aug 08 2007 Jarod Wilson <jwilson@redhat.com> 1.3-0.1.beta1
- Update to rrdtool 1.3 beta1

* Tue Jul 10 2007 Jarod Wilson <jwilson@redhat.com> 1.2.999-0.3.r1144
- Update to latest rrdtool pre-1.3 svn snapshot (svn r1144)
- Add php abi check (Resolves: #247339)

* Fri Jun 15 2007 Jarod Wilson <jwilson@redhat.com> 1.2.999-0.2.r1127
- Fix up BuildRequires

* Fri Jun 15 2007 Jarod Wilson <jwilson@redhat.com> 1.2.999-0.1.r1127
- Update to rrdtool pre-1.3 svn snapshot (svn r1127)

* Mon May 21 2007 Jarod Wilson <jwilson@redhat.com> 1.2.23-5
- BR: ruby so %%ruby_sitearch gets set

* Mon May 21 2007 Jarod Wilson <jwilson@redhat.com> 1.2.23-4
- Build ruby bindings

* Thu May 03 2007 Jarod Wilson <jwilson@redhat.com> 1.2.23-3
- Disable php bits on ppc64 for now, they fail to build

* Thu May 03 2007 Jarod Wilson <jwilson@redhat.com> 1.2.23-2
- Add BR: perl-devel for Fedora 7 and later

* Tue May 01 2007 Jarod Wilson <jwilson@redhat.com> 1.2.23-1
- New upstream release

* Tue May 01 2007 Jarod Wilson <jwilson@redhat.com> 1.2.21-1
- New upstream release

* Wed Apr 25 2007 Jarod Wilson <jwilson@redhat.com> 1.2.19-2
- Define %%python_version *before* its needed (#237826)

* Mon Apr 09 2007 Jarod Wilson <jwilson@redhat.com> 1.2.19-1
- New upstream release

* Tue Jan 23 2007 Jarod Wilson <jwilson@redhat.com> 1.2.18-1
- New upstream release

* Mon Jan 22 2007 Jarod Wilson <jwilson@redhat.com> 1.2.17-1
- New upstream release

* Tue Jan 02 2007 Jarod Wilson <jwilson@redhat.com> 1.2.15-9
- Fix crash with long error strings (upstream
  changesets 929 and 935)

* Thu Dec 14 2006 Jarod Wilson <jwilson@redhat.com> 1.2.15-8
- Fix for log grid memory leak (#201241)

* Tue Dec 12 2006 Jarod Wilson <jwilson@redhat.com> 1.2.15-7
- Rebuild for python 2.5

* Tue Nov 14 2006 Jarod Wilson <jwilson@redhat.com> 1.2.15-6
- Conditionalize python, php and tcl bits (Resolves #203275)

* Wed Oct 25 2006 Jarod Wilson <jwilson@redhat.com> 1.2.15-5
- Add tcl sub-package (#203275)

* Tue Sep 05 2006 Jarod Wilson <jwilson@redhat.com> 1.2.15-4
- Rebuild for new glibc

* Wed Aug 02 2006 Jarod Wilson <jwilson@redhat.com> 1.2.15-3
- One more addition to initrrdtool patch, to fully revert
  and correct upstream changeset 839
- Fix for no python in minimal fc4 buildroots

* Tue Aug  1 2006 Mihai Ibanescu <misa@redhat.com> 1.2.15-2
- Fixed rrdtool-python to import the module properly (patch
  rrdtool-1.2.15-initrrdtool.patch)

* Mon Jul 17 2006 Jarod Wilson <jwilson@redhat.com> 1.2.15-1
- Update to 1.2.15
- Minor spec cleanups

* Sat Jun 24 2006 Jarod Wilson <jwilson@redhat.com> 1.2.13-7
- Fix up Obsoletes

* Mon Jun 19 2006 Jarod Wilson <jwilson@redhat.com> 1.2.13-6
- Flip perl, php and python sub-package names around to 
  conform with general practices

* Sat Jun 10 2006 Jarod Wilson <jwilson@redhat.com> 1.2.13-5
- Minor fixes to make package own created directories

* Wed Jun 07 2006 Jarod Wilson <jwilson@redhat.com> 1.2.13-4
- Add php bits back into the mix

* Mon Jun 05 2006 Jarod Wilson <jwilson@redhat.com> 1.2.13-3
- Merge spec fixes from bz 185909

* Sun Jun 04 2006 Jarod Wilson <jwilson@redhat.com> 1.2.13-2
- Remove explicit perl dep, version grabbing using rpm during
  rpmbuild not guaranteed to work (fails on ppc in plague),
  and auto-gen perl deps are sufficient

* Sat Jun 03 2006 Jarod Wilson <jwilson@redhat.com> 1.2.13-1
- Update to release 1.2.13
- Merge spec changes from dag, atrpms and mdk builds
- Additional hacktastic contortions for lib64 & rpath messiness
- Add missing post/postun ldconfig
- Fix a bunch of rpmlint errors
- Disable static libs, per FE guidelines
- Split off docs

* Wed Apr 19 2006 Chris Ricker <kaboom@oobleck.net> 1.2.12-1
- Rev to 1.2

* Fri May 20 2005 Matthias Saou <http://freshrpms.net/> 1.0.49-5
- Include patch from Michael to fix perl module compilation on FC4 (#156242).

* Fri May 20 2005 Matthias Saou <http://freshrpms.net/> 1.0.49-4
- Fix for the php module patch (Joe Pruett, Dag Wieers), #156716.
- Update source URL to new location since 1.2 is now the default stable.
- Don't (yet) update to 1.0.50, as it introduces some changes in the perl
  modules install.

* Mon Jan 31 2005 Matthias Saou <http://freshrpms.net/> 1.0.49-3
- Put perl modules in vendor_perl and not site_perl. #146513

* Thu Jan 13 2005 Matthias Saou <http://freshrpms.net/> 1.0.49-2
- Minor cleanups.

* Wed Aug 25 2004 Dag Wieers <dag@wieers.com> - 1.0.49-1
- Updated to release 1.0.49.

* Wed Aug 25 2004 Dag Wieers <dag@wieers.com> - 1.0.48-3
- Fixes for x86_64. (Garrick Staples)

* Fri Jul  2 2004 Matthias Saou <http://freshrpms.net/> 1.0.48-3
- Actually apply the patch for fixing the php module, doh!

* Thu May 27 2004 Matthias Saou <http://freshrpms.net/> 1.0.48-2
- Added php.d config entry to load the module once installed.

* Thu May 13 2004 Dag Wieers <dag@wieers.com> - 1.0.48-1
- Updated to release 1.0.48.

* Tue Apr 06 2004 Dag Wieers <dag@wieers.com> - 1.0.47-1
- Updated to release 1.0.47.

* Thu Mar  4 2004 Matthias Saou <http://freshrpms.net/> 1.0.46-2
- Change the strict dependency on perl to fix problem with the recent
  update.

* Mon Jan  5 2004 Matthias Saou <http://freshrpms.net/> 1.0.46-1
- Update to 1.0.46.
- Use system libpng and zlib instead of bundled ones.
- Added php-rrdtool sub-package for the php4 module.

* Fri Dec  5 2003 Matthias Saou <http://freshrpms.net/> 1.0.45-4
- Added epoch to the perl dependency to work with rpm > 4.2.
- Fixed the %% escaping in the perl dep.

* Mon Nov 17 2003 Matthias Saou <http://freshrpms.net/> 1.0.45-2
- Rebuild for Fedora Core 1.

* Sun Aug  3 2003 Matthias Saou <http://freshrpms.net/>
- Update to 1.0.45.

* Wed Apr 16 2003 Matthias Saou <http://freshrpms.net/>
- Update to 1.0.42.

* Mon Mar 31 2003 Matthias Saou <http://freshrpms.net/>
- Rebuilt for Red Hat Linux 9.

* Wed Mar  5 2003 Matthias Saou <http://freshrpms.net/>
- Added explicit perl version dependency.

* Sun Feb 23 2003 Matthias Saou <http://freshrpms.net/>
- Update to 1.0.41.

* Fri Jan 31 2003 Matthias Saou <http://freshrpms.net/>
- Update to 1.0.40.
- Spec file cleanup.

* Fri Jul 05 2002 Henri Gomez <hgomez@users.sourceforge.net>
- 1.0.39

* Mon Jun 03 2002 Henri Gomez <hgomez@users.sourceforge.net>
- 1.0.38

* Fri Apr 19 2002 Henri Gomez <hgomez@users.sourceforge.net>
- 1.0.37

* Tue Mar 12 2002 Henri Gomez <hgomez@users.sourceforge.net>
- 1.0.34
- rrdtools include zlib 1.1.4 which fix vulnerabilities in 1.1.3

