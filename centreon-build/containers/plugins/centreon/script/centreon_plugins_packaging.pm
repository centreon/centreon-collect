
package centreon::script::centreon_plugins_packaging;

use strict;
use warnings;
use centreon::script;
use File::Copy::Recursive qw(dircopy);
use File::Path;
use FindBin;
use Data::Dumper;

use base qw(centreon::script);

sub new {
    my $class = shift;
    my $self = $class->SUPER::new("centreon_plugins_packaging",
        centreon_db_conn => 0,
        centstorage_db_conn => 0,
        noconfig => 1
    );
    bless $self, $class;
    $self->add_options(
        "filter-package:s" => \$self->{filter_package},
        "force-create"     => \$self->{force_create},
    );

    $self->{rpm_plugin_path} = '/usr/lib/centreon/plugins/';
    $self->{git_url_packaging} = 'http://gitbot:gitbot@git.int.centreon.com/centreon-plugins-packaging';
    $self->{git_dir_packaging} = 'centreon-plugins-packaging';
    $self->{git_url_centreon_plugins} = 'https://gitbot:gitbot@github.com/centreon/centreon-plugins.git';
    $self->{git_dir_centreon_plugins} = 'centreon-plugins';
	$self->{git_url_centreon_automation} = 'https://centreonbot:518bc6ce608956da1eadbe71ff7de731474b773b@github.com/centreon/centreon-automation.git';
    $self->{git_dir_centreon_automation} = 'centreon-automation';
    $self->{build_dir} = $FindBin::Bin . '/build';
	$self->{rpm_save_directory} = $FindBin::Bin . '/' . $self->{git_dir_centreon_plugins} . '/';
	$self->{unstable_internal_repo} = '/srv/repos/standard/3.4/';
	$self->{unstable_internal_repo_addr} = 'srvi-ces-repository.int.centreon.com';
    $self->{timeout_git_clone} = 300;
    
    $self->{git_rm_plugins} = [];
    return $self;
}

sub init {
    my ($self) = @_;
    $self->SUPER::init();
    
    centreon::common::misc::mymodule_load(logger => $self->{logger}, module => 'App::FatPacker',
                                          error_msg => "Cannot load module 'App::FatPacker'.");
    centreon::common::misc::mymodule_load(logger => $self->{logger}, module => 'JSON',
                                          error_msg => "Cannot load module 'JSON'.");
    centreon::common::misc::mymodule_load(logger => $self->{logger}, module => 'File::Copy::Recursive',
                                          error_msg => "Cannot load module 'File::Copy::Recursive'.");
}

sub get_repositories {
    my ($self, %options) = @_;
    my $err;
    my ($lerror, $stdout, $exit_code);
    
    $self->{logger}->writeLogInfo("Create build tree");
    File::Path::make_path($self->{build_dir} . '/repos', { error => \$err });
    centreon::common::misc::path_errors(logger => $self->{logger}, err => $err, prefix => "create repositories path");
    File::Path::make_path($self->{build_dir} . '/rpmbuild',
                          $self->{build_dir} . '/rpmbuild/BUILD',
                          $self->{build_dir} . '/rpmbuild/BUILDROOT',
                          $self->{build_dir} . '/rpmbuild/RPMS',
                          $self->{build_dir} . '/rpmbuild/SOURCES',
                          $self->{build_dir} . '/rpmbuild/SPECS',
                          $self->{build_dir} . '/rpmbuild/SRPMS',
                          { error => \$err });
    centreon::common::misc::path_errors(logger => $self->{logger}, err => $err, prefix => "create rpmbuild path");
   
	$self->{logger}->writeLogInfo("Create RPM backup directory");
	File::Path::make_path($self->{rpm_save_directory},
						  $self->{rpm_save_directory} . 'el6',
						  $self->{rpm_save_directory} . 'el7',
						  { error => \$err });
	centreon::common::misc::path_errors(logger => $self->{logger}, err => $err, prefix => "create rpm save path");

	$self->{logger}->writeLogInfo("Clone packaging repository");
	($lerror, $stdout, $exit_code) = 
		centreon::common::misc::backtick(command => 'git clone ' . $self->{git_url_packaging} . ' ' . 
			                             $self->{build_dir} . '/repos/' . $self->{git_dir_packaging},
				                         logger => $self->{logger},
				                         timeout => $self->{timeout_git_clone},
				                         wait_exit => 1,
					                    );
	if ($lerror != 0 && $exit_code != 0) {
	    $self->{logger}->writeLogError("problem to clone packaging repository");
	    exit(1);
	}

	$self->{logger}->writeLogInfo("Clone centreon-plugins repository");
	($lerror, $stdout, $exit_code) = 
		centreon::common::misc::backtick(command => 'git clone ' . $self->{git_url_centreon_plugins} . ' ' . 
			                             $self->{build_dir} . '/repos/' . $self->{git_dir_centreon_plugins},
			                             logger => $self->{logger},
			                             timeout => $self->{timeout_git_clone},
			                             wait_exit => 1,
				                        );
	if ($lerror != 0 && $exit_code != 0) {
	    $self->{logger}->writeLogError("problem to clone centreon-plugins repository");
	    exit(1);
	}
    
	$self->{logger}->writeLogInfo("Get last tag from centreon-plugins");
    centreon::common::misc::chdir(logger => $self->{logger}, dir => $self->{build_dir} . '/repos/' . $self->{git_dir_centreon_plugins});

    ($lerror, $stdout, $exit_code) = 
        centreon::common::misc::backtick(command => 'git tag',
                                         logger => $self->{logger},
                                         timeout => 20,
                                         wait_exit => 1,
                                        );
    if ($lerror != 0 && $exit_code != 0) {
        $self->{logger}->writeLogError("problem to get tag from centreon-plugins repository");
        exit(1);
    }
   
    for my $tag (sort {$b <=> $a} grep(/^[0-9]{8}$/, split /\n/, $stdout)) {
        $self->{newest_tag} = $tag;
        last;
    }
    
    if (!defined($self->{newest_tag})) {
        $self->{logger}->writeLogError("cannot find newest tag from centreon-plugins repository");
        exit(1);
    }
    
    $self->{logger}->writeLogInfo("find tag : " . $self->{newest_tag});
    
    $self->{logger}->writeLogInfo("Get checkout tag from centreon-plugins");
    ($lerror, $stdout, $exit_code) = 
        centreon::common::misc::backtick(command => 'git checkout ' . $self->{newest_tag},
                                         logger => $self->{logger},
                                         timeout => 30,
                                         wait_exit => 1,
                                        );
    if ($lerror != 0 && $exit_code != 0) {
        $self->{logger}->writeLogError("problem to checkout tag from centreon-plugins repository");
        exit(1);
    }

	$self->{logger}->writeLogInfo("Clone centreon-automation  repository");
	($lerror, $stdout, $exit_code) =
		centreon::common::misc::backtick(command => 'git clone ' . $self->{git_url_centreon_automation} . ' ' .
										 $self->{build_dir} . '/repos/' . $self->{git_dir_centreon_automation},
										 logger => $self->{logger},
										 timeout => $self->{timeout_git_clone},
										 wait_exit => 1,
										);
	if ($lerror != 0 && $exit_code != 0) {
		$self->{logger}->writeLogError("problem to clone centreon-automation repository");
		exit(1);
	}

	unless (dircopy($self->{build_dir} . '/repos/' . $self->{git_dir_centreon_automation} . "/plugins",
		$self->{build_dir} . '/repos/' . $self->{git_dir_centreon_plugins})) {

		$self->{logger}->writeLogError("problem to move ".$self->{build_dir} . '/repos/' . $self->{git_dir_centreon_automation} . "/plugins/discovery to " . $self->{build_dir} . '/repos/' . $self->{git_dir_centreon_plugins});
		exit(1);
	}
}

sub pkg_file {
    my ($self, %options) = @_;
    my $fh;

    my ($code, $json_content) = centreon::common::misc::read_file(file => $options{json_file});
    if ($code == 1) {
        $self->{logger}->writeLogError($json_content);
        return 1;
    }
    
    eval {
        $self->{pkg_data} = JSON::decode_json($json_content);
    };
    if ($@) {
        $self->{logger}->writeLogError("Cannot decode json response: " . $@);
        return 1;
    }
    
    if (!defined($self->{pkg_data}->{pkg_name}) || $self->{pkg_data}->{pkg_name} eq '') {
        $self->{logger}->writeLogError("please need a 'pkg_name' in json file");
        return 1;
    }
    if (!defined($self->{pkg_data}->{plugin_name}) || $self->{pkg_data}->{plugin_name} eq '') {
        $self->{logger}->writeLogError("please need a 'plugin_name' in json file");
        return 1;
    }
    if (!defined($self->{pkg_data}->{files}) || ref($self->{pkg_data}->{files}) ne 'ARRAY' || 
        scalar(@{$self->{pkg_data}->{files}}) <= 0) {
        $self->{logger}->writeLogError("please need a 'files' array in json file");
        return 1;
    }
    
    return 0;
}

sub set_fatpacker_option {
    my ($self, %options) = @_;
    my ($code, $content);

    ($code, $content) = centreon::common::misc::read_file(file => $self->{build_dir} . '/repos/' . $self->{git_dir_centreon_plugins} . '/lib/centreon/plugins/script.pm');
    if ($code == 1) {
        $self->{logger}->writeLogError("cannot change fatpacker option: " . $content);
        return 1;
    }
    
    $content =~ s/\$alternative_fatpacker = 0;/\$alternative_fatpacker = 1;/ms;
    
    ($code, $content) = centreon::common::misc::write_file(file => $self->{build_dir} . '/repos/' . $self->{git_dir_centreon_plugins} . '/lib/centreon/plugins/script.pm',
                                                           content => $content);
    if ($code == 1) {
        $self->{logger}->writeLogError("cannot change fatpacker option: " . $content);
        return 1;
    }
                                                           
    return 0;
}

sub do_fatpack {
    my ($self, %options) = @_;
    my ($fh, $err);
    
    $self->{logger}->writeLogInfo("fatpacker create: begin");
    
    File::Path::remove_tree($self->{build_dir} . '/repos/' . $self->{git_dir_centreon_plugins} . '/lib/', { error => \$err });
    return 1 if (centreon::common::misc::path_errors(logger => $self->{logger}, err => $err, prefix => "delete lib directory for fatpacker", no_quit => 1) == 1);
    File::Path::make_path($self->{build_dir} . '/repos/' . $self->{git_dir_centreon_plugins} . '/lib/', { error => \$err });
    return 1 if (centreon::common::misc::path_errors(logger => $self->{logger}, err => $err, prefix => "create repositories path for fatpacker", no_quit => 1) == 1);
    
    my @common_files = ('centreon/plugins/misc.pm', 'centreon/plugins/mode.pm', 'centreon/plugins/options.pm',
                        'centreon/plugins/output.pm', 'centreon/plugins/perfdata.pm', 'centreon/plugins/script.pm',
                        'centreon/plugins/statefile.pm', 'centreon/plugins/values.pm',
                        'centreon/plugins/alternative/Getopt.pm', 'centreon/plugins/alternative/FatPackerOptions.pm', 
                        'centreon/plugins/templates/counter.pm', 'centreon/plugins/templates/hardware.pm');
    foreach my $file ((@common_files, @{$self->{pkg_data}->{files}})) {
        if (-f $self->{build_dir} . '/repos/' . $self->{git_dir_centreon_plugins} . '/' . $file) {
            if (!File::Copy::Recursive::fcopy($self->{build_dir} . '/repos/' . $self->{git_dir_centreon_plugins} . '/' . $file, 
                                              $self->{build_dir} . '/repos/' . $self->{git_dir_centreon_plugins} . '/lib/' . $file)) {
                $self->{logger}->writeLogError("cannot copy file: " . $!);
                return 1;
            }
        } elsif (-d $self->{build_dir} . '/repos/' . $self->{git_dir_centreon_plugins} . '/' . $file) {
            if (!File::Copy::Recursive::dircopy($self->{build_dir} . '/repos/' . $self->{git_dir_centreon_plugins} . '/' . $file, 
                                                $self->{build_dir} . '/repos/' . $self->{git_dir_centreon_plugins} . '/lib/' . $file)) {
                $self->{logger}->writeLogError("cannot copy dir: " . $!);
                return 1;
            }
        } else {
            $self->{logger}->writeLogError("cannot find directory/file in centreon-plugins repository: " . $file);
            return 1;
        }
    }
    
    $self->{logger}->writeLogInfo("fatpacker create: copy success");
    
    return 1 if ($self->set_fatpacker_option() == 1);
    
    # We create the fatpack
    # We are in good directory already
    my $fatpacker = App::FatPacker->new();
    my $content = $fatpacker->fatpack_file("centreon_plugins.pl");
    
    # we create the file
    if (!open($fh, '>', $options{path} . '/plugin/' . $self->{pkg_data}->{plugin_name} . '.new')) {
        $self->{logger}->writeLogError("could not open file: " . $!);
        return 1;
    }
    print $fh $content;
    close $fh;
    
    $self->{logger}->writeLogInfo("fatpacker create: finished");
    return 0;
}

sub check_last_plugin {
    my ($self, %options) = @_;
    my ($code, $content);

    $self->{last_plugin_data} = undef;
    $self->{version_rpm} = undef;
    $self->{release_rpm} = 1;
    return 1 if (! -f $options{path} . '/plugin/last_plugin.json');
        
    ($code, $content) = centreon::common::misc::read_file(file => $options{path} . '/plugin/last_plugin.json');
    if ($code == 1) {
        $self->{logger}->writeLogError($content);
        return -1;
    }
    
    eval {
        $self->{last_plugin_data} = JSON::decode_json($content);
    };
    if ($@) {
        $self->{logger}->writeLogError("Cannot decode last_plugin.json response: " . $@);
        return -1;
    }
    
    $self->{release_rpm} = $self->{last_plugin_data}->{release_rpm};
    $self->{version_rpm} = $self->{last_plugin_data}->{version_rpm};
    if (defined($self->{last_plugin_data}->{plugin_name}) &&  
        $self->{last_plugin_data}->{plugin_name} ne $self->{pkg_data}->{plugin_name}) {
        $self->{logger}->writeLogInfo("new plugin_name = " . $self->{pkg_data}->{plugin_name} . " [old = $self->{last_plugin_data}->{plugin_name}]");
        return 0;
    }
    if (defined($self->{last_plugin_data}->{pkg_name}) &&  
        $self->{last_plugin_data}->{pkg_name} ne $self->{pkg_data}->{pkg_name}) {
        $self->{logger}->writeLogInfo("new pkg_name = " . $self->{pkg_data}->{pkg_name} . " [old = $self->{last_plugin_data}->{pkg_name}]");
        return 0;
    }
    $self->{last_plugin_data}->{rhel_dependencies} = [] if (!defined($self->{last_plugin_data}->{rhel_dependencies}));
    $self->{pkg_data}->{rhel_dependencies} = [] if (!defined($self->{pkg_data}->{rhel_dependencies}));
    if (join('-', @{$self->{last_plugin_data}->{rhel_dependencies}}) ne join('-', @{$self->{pkg_data}->{rhel_dependencies}})) {
        $self->{logger}->writeLogInfo("change dependencies");
        return 0;
    }

    return 1;
}

sub is_plugin_update {
    my ($self, %options) = @_;
    my ($err, $lerror, $stdout, $exit_code);
    my $need_diff = 1;

    $self->{logger}->writeLogInfo("plugin update: begin");
    
    my $last_plugin_ret = $self->check_last_plugin(%options);
    return -1 if ($last_plugin_ret == -1);
    
    $self->{create_package} = 0;
    File::Path::make_path($options{path} . '/plugin/', { error => \$err });
    centreon::common::misc::path_errors(logger => $self->{logger}, err => $err, prefix => "create repositories path");
    
    if (!defined($self->{last_plugin_data}) || 
        ! -f $options{path} . '/plugin/' . $self->{last_plugin_data}->{plugin_name} . '.current') {
        $self->{logger}->writeLogInfo("plugin update: no current plugin");
        $self->{create_package} = 1;
        $self->{version_rpm} = $self->{newest_tag};
        $need_diff = 0;
    }
    
    # We'll erase and create it
    if (defined($self->{force_create})) {
        $need_diff = 0;
    }
    
    if ($self->do_fatpack(path => $options{path}) != 0) {
        return -1;
    }
    
    if ($need_diff == 1) {
        ($lerror, $stdout, $exit_code) = 
            centreon::common::misc::backtick(command => 'diff --suppress-common-lines -b "' . 
                                                $options{path} . '/plugin/' . $self->{last_plugin_data}->{plugin_name} . '.current" "' .
                                                $options{path} . '/plugin/' . $self->{pkg_data}->{plugin_name} . '.new"',
                                             logger => $self->{logger},
                                             timeout => 30,
                                             wait_exit => 1,
                                             );
        if ($lerror != 0 || $exit_code > 1) {
            $self->{logger}->writeLogError("plugin update: problem to execute diff command");
            unlink($options{path} . '/plugin/' . $self->{pkg_data}->{plugin_name} . '.new');
            return -1;
        }
        my @lines = split /\n/, $stdout;
        if ($exit_code == 1 && (scalar(@lines) > 4 || $stdout !~ /\$global_version/ms)) {
            $self->{create_package} = 1;
            # If the version has changed
            # we go back release at one
            if ($self->{newest_tag} ne $self->{version_rpm}) {
                $self->{version_rpm} = $self->{newest_tag};
                $self->{release_rpm} = 1;
            }
        } else {
            $self->{logger}->writeLogError("plugin update: no source code change");
        }
    }
    
    # We need to look for some small updates
    if ($self->{create_package} == 0 && $last_plugin_ret == 0) {
        $self->{create_package} = 1;
        $self->{release_rpm}++;
    }
    
    if ($self->{create_package} == 1 || defined($self->{force_create})) {
        if (defined($self->{last_plugin_data}->{plugin_name}) && $self->{last_plugin_data}->{plugin_name} ne $self->{pkg_data}->{plugin_name}) {
            push @{$self->{git_rm_plugins}}, $self->{last_plugin_data}->{plugin_name};
        } else {
            unlink($options{path} . '/plugin/' . $self->{last_plugin_data}->{plugin_name} . '.current') if (defined($self->{last_plugin_data}->{plugin_name}));
        }
        rename($options{path} . '/plugin/' . $self->{pkg_data}->{plugin_name} . '.new', 
               $options{path} . '/plugin/' . $self->{pkg_data}->{plugin_name} . '.current');
        return 0;
    }
    
    unlink($options{path} . '/plugin/' . $self->{pkg_data}->{plugin_name} . '.new');
    return 1;
}

sub save_last_plugin {
    my ($self, %options) = @_;
    my $last_plugin_data = {};

    $self->{logger}->writeLogInfo("save last plugin: begin");    
    $last_plugin_data->{release_rpm} = $self->{release_rpm};
    $last_plugin_data->{version_rpm} = $self->{version_rpm};
    $last_plugin_data->{rhel_dependencies} = $self->{pkg_data}->{rhel_dependencies};
    $last_plugin_data->{plugin_name} = $self->{pkg_data}->{plugin_name};
    $last_plugin_data->{last_plugin_data} = $self->{pkg_data}->{pkg_name};
    
    my $json;
    eval {
        $json = JSON::encode_json($last_plugin_data);
    };
    if ($@) {
        $self->{logger}->writeLogError("Cannot encode last_plugin.json response: " . $@);
        return -1;
    }
    
    my ($code, $content) = centreon::common::misc::write_file(file => $options{path} . '/plugin/last_plugin.json',
                                                              content => $json);
    if ($code == 1) {
        $self->{logger}->writeLogError($content);
        return 1;
    }
    
    return 0;
}

sub build_rpm_el {
    my ($self, %options) = @_;
    
    $self->{logger}->writeLogError("build rpm $options{dist}: begin");
    my $requires = '';
    if (defined($self->{pkg_data}->{'rhel_dependencies_' . $options{dist}})) {
        foreach my $dep (@{$self->{pkg_data}->{'rhel_dependencies_' .  $options{dist}}}) {
            $requires .= "Requires: $dep\n";
        }
    } elsif (defined($self->{pkg_data}->{rhel_dependencies})) {
        foreach my $dep (@{$self->{pkg_data}->{rhel_dependencies}}) {
            $requires .= "Requires: $dep\n";
        }
    }
	my $obsoletes = '';
	if ($self->{pkg_data}->{pkg_name} =~ m/centreon-plugin-(.*)/) {
		$obsoletes = 'Obsoletes: ces-plugins-'.$1;
	}
    
    my $spec_tpl = <<"END_MSG";
Name:		$self->{pkg_data}->{pkg_name}
Version:	$self->{version_rpm}
Release:	$self->{release_rpm}\%{?dist}
Summary:	Centreon plugin
Group:		Development/Libraries
License:	ASL 2.0
URL:		https://www.centreon.com/
BuildArch:  noarch
BuildRoot:	\%{_tmppath}/\%{name}-\%{version}-\%{release}-root-\%(\%{__id_u} -n)
AutoReqProv:    no

$obsoletes

$requires

\%description

\%prep

\%build

\%install
rm -rf \%{buildroot}
mkdir -p \%{buildroot}/usr/lib/centreon/plugins/
\%{__install} -m 775 $options{path}/plugin/$self->{pkg_data}->{plugin_name}.current \%{buildroot}$self->{rpm_plugin_path}/$self->{pkg_data}->{plugin_name}

\%clean
rm -rf \%{buildroot}

\%files
\%defattr(-,root,root,-)
$self->{rpm_plugin_path}
END_MSG

    my ($code, $content) = centreon::common::misc::write_file(file => $self->{build_dir} . '/rpmbuild/SPECS/pp.spec',
                                                              content => $spec_tpl);
    if ($code == 1) {
        $self->{logger}->writeLogError($content);
        return 1;
    }
    
    my ($lerror, $stdout, $exit_code) = 
            centreon::common::misc::backtick(command => 'rpmbuild --define "dist .' . $options{dist} . '" --define "_topdir ' . $self->{build_dir} . '/rpmbuild/" -ba  "' . $self->{build_dir} . '/rpmbuild/SPECS/pp.spec"',
                                             logger => $self->{logger},
                                             timeout => 60,
                                             wait_exit => 1,
                                             );
    if ($lerror != 0 || $exit_code > 1) {
        $self->{logger}->writeLogError("build rpm $options{dist}: rpmbuild error");
        return -1;
    }
    
    my $rpm_name = $self->{pkg_data}->{pkg_name} . '-' . $self->{version_rpm} . '-' . $self->{release_rpm} . '.' . $options{dist} . '.noarch.rpm';
	File::Copy::Recursive::fcopy($self->{build_dir} . '/rpmbuild/RPMS/noarch/' . $rpm_name, 
								$self->{rpm_save_directory} . $options{dist});
    $self->{logger}->writeLogError("build rpm $options{dist}: success " . $rpm_name);
    return 0;
}

sub build_rpm {
    my ($self, %options) = @_;

    $self->build_rpm_el(%options, dist => 'el6');
    $self->build_rpm_el(%options, dist => 'el7');
}

sub do_package {
    my ($self, %options) = @_;
    
    if (! -f $options{path} . '/pkg.json' || ! -r $options{path} . '/pkg.json') {
        $self->{logger}->writeLogError("cannot find or read file pkg.json file");
        return 1;
    }
    
    return 1 if ($self->pkg_file(json_file => $options{path} . '/pkg.json') == 1);
    $self->{logger}->writeLogInfo("pkg.json loaded: successfully");
    
    return 1 if ($self->is_plugin_update(path => $options{path}) != 0);
    $self->{logger}->writeLogInfo("plugin update: yes!! [version: $self->{version_rpm}] [release: $self->{release_rpm}]");
    
    return 1 if ($self->save_last_plugin(path => $options{path}) != 0);
    $self->{logger}->writeLogInfo("save last plugin: successfully");
    
    $self->build_rpm(path => $options{path});
    
    return 0;
}

sub build_packages {
    my ($self, %options) = @_;
    my $dh;
    
    $self->{logger}->writeLogInfo("List packages");
    if (!opendir($dh, $self->{build_dir} . '/repos/' . $self->{git_dir_packaging})) {
        $self->{logger}->writeLogError("can't opendir: " . $!);
        exit(1);
    }
    
    while (my $dir_name = readdir $dh) {
        my $abs_dir = $self->{build_dir} . '/repos/' . $self->{git_dir_packaging} . '/' . $dir_name;
        if (! -d $abs_dir) {
            $self->{logger}->writeLogInfo("skipping " . $abs_dir . " : not a directory");
            next;
        }
        if ($dir_name =~ /^\./) {
            $self->{logger}->writeLogInfo("skipping " . $abs_dir);
            next;
        }
        
        if (defined($self->{filter_package}) && $self->{filter_package} ne '' &&
            $dir_name !~ /$self->{filter_package}/) {
            $self->{logger}->writeLogInfo("skipping " . $abs_dir . " : not matching filter");
            next;
        }
        
        $self->{logger}->writeLogInfo("=== Package : " . $dir_name);
        $self->do_package(path => $abs_dir, name => $dir_name);
        $self->{logger}->writeLogInfo("========================");
    }
    
    closedir $dh;
}

sub git_commit_changes {
    my ($self, %options) = @_;

    centreon::common::misc::chdir(logger => $self->{logger}, dir => $self->{build_dir} . '/repos/' . $self->{git_dir_packaging});
    $self->{logger}->writeLogInfo("commit changes: begin");
    my $command = 'git status';

   my ($lerror, $stdout, $exit_code) =
           centreon::common::misc::backtick(command => $command,
                                             logger => $self->{logger},
                                             timeout => 60,
                                             wait_exit => 1,
                                             );
    if ($lerror != 0 || $exit_code > 1) {
        $self->{logger}->writeLogError("commit changes: error");
        return -1;
    }

    if ($stdout !~ /nothing to commit/) {
        $command = '';

        if (scalar(@{$self->{git_rm_plugins}}) > 0) {
            $command .= 'git rm ' . join(' ',  @{$self->{git_rm_plugins}}) . ' && ';
        }
        $command .= "git add . && git commit -m 'update release' && git push";
        my ($lerror, $stdout, $exit_code) = 
                centreon::common::misc::backtick(command => $command,
                                                 logger => $self->{logger},
                                                 timeout => 60,
                                                 wait_exit => 1,
                                                 );
        if ($lerror != 0 || $exit_code > 1) {
            $self->{logger}->writeLogError("commit changes: error");
            return -1;
       }
    }
    
    $self->{logger}->writeLogInfo("commit changes: success");
}

sub export_rpm_internal_repo_el {
	my ($self, %options) = @_;

        opendir (DIR, $self->{rpm_save_directory} . $options{dist}) or die "Cannot open directory: $!";
        my @files = grep ! m/^\./ && ! /config_file/, readdir DIR; # skip hidden files and config files
        closedir(DIR);

        return -1 if ($#files < 0);

	$self->{logger}->writeLogInfo("Export ".$options{dist}."  RPM to internal unstable repository");
	my $command .= 'scp '. $self->{rpm_save_directory} . $options{dist} . '/*.rpm root@' . $self->{unstable_internal_repo_addr} . 
		':' . $self->{unstable_internal_repo} . $options{dist} . '/unstable/noarch/RPMS/';
	my ($lerror, $stdout, $exit_code) =
		centreon::common::misc::backtick(command => $command,
		                                 logger => $self->{logger},
		                                 timeout => 60,
		                                 wait_exit => 1,
		                                );
	if ($lerror != 0 || $exit_code > 1) {
		$self->{logger}->writeLogError("Export ".$options{dist}." RPM to internal unstable repo: error");
		return -1;
	}
}

sub export_rpm_internal_repo {
    my ($self, %options) = @_;

    $self->export_rpm_internal_repo_el(%options, dist => 'el6');
    $self->export_rpm_internal_repo_el(%options, dist => 'el7');
}

sub remove_old_rpm_el {
	my ($self, %options) = @_;

	# Get list of centreon-plugin on unstable repo
	my $command = 'ssh ' . $self->{unstable_internal_repo_addr} . 
		' \'ls ' . $self->{unstable_internal_repo} . $options{dist} . '/unstable/noarch/RPMS/ | egrep -e  "centreon-plugin-[A-Za-z0-9\-]+"\'';
    my ($lerror, $stdout, $exit_code) =
		centreon::common::misc::backtick(command => $command,
										 logger => $self->{logger},
										 timeout => 60,
										 wait_exit => 1,
										);
	if ($lerror != 0 || $exit_code > 1) {
		$self->{logger}->writeLogError("get list of ".$options{dist}." internal unstable repo rpm to delete: error");
		return -1;
	}
	if (defined($stdout) &&  $stdout !~ '/^$/') {
		# Parse list of centreon-plugin to detect files must be deleted
		my (%plugins, @plugins_to_delete);
		foreach my $plugin (split('\n', $stdout)) {
			if ($plugin =~ /(centreon-plugin-[A-Za-z0-9\-]+)\-\d{8}\-\d+\.$options{dist}\.noarch\.rpm/) {
				if (defined($plugins{$1})) {
					push (@plugins_to_delete, $plugins{$1});
					$plugins{$1} = $plugin;
				} else {
					$plugins{$1} = $plugin;
				}
			}
		}

		# Delete old files
		if (@plugins_to_delete && $#plugins_to_delete > 0) {
			print "nb element: $#plugins_to_delete\n";
			my $fileToDelete = join(' ', @plugins_to_delete);
		
			$self->{logger}->writeLogError("delete files: ".$fileToDelete);
			$command = 'ssh ' . $self->{unstable_internal_repo_addr} . 
				' \'cd ' . $self->{unstable_internal_repo} . $options{dist} . '/unstable/noarch/RPMS/ && rm -f ' . $fileToDelete .'\'';
			my ($lerror, $stdout, $exit_code) =
				centreon::common::misc::backtick(command => $command,
				                                 logger => $self->{logger},
												 timeout => 60,
												 wait_exit => 1,
												);
			if ($lerror != 0 || $exit_code > 1) {
				$self->{logger}->writeLogError("clean ".$options{dist}." internal unstable repo: erro");
				return -1;
			}
		}
	}
}

sub remove_old_rpm {
    my ($self, %options) = @_;

	$self->remove_old_rpm_el(%options, dist => 'el6');
	$self->remove_old_rpm_el(%options, dist => 'el7');
}

sub run {
    my ($self) = @_;

    $self->SUPER::run();

    $self->{logger}->writeLogInfo("Script begins");
    $self->get_repositories();
    
    $self->build_packages();
    
    $self->git_commit_changes();

	$self->export_rpm_internal_repo();
	$self->remove_old_rpm();

	$self->{logger}->writeLogInfo("End of script");
    exit(0);
}

sub DESTROY {
    my ($self) = @_;
    my $err;
    
    $self->SUPER::DESTROY();
    centreon::common::misc::chdir(logger => $self->{logger}, dir => $FindBin::Bin);
    File::Path::remove_tree($self->{build_dir}, { error => \$err });
    centreon::common::misc::path_errors(logger => $self->{logger}, err => $err, prefix => "delete build directory");
    $self->{logger}->writeLogInfo("Build directory removed");
}

1;
