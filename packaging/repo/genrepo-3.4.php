<?php

// Base centreon version.
$centreonversion = '3.4';
$reporelease = '4';

// Information table.
$repos = array(
    '' => array(
        'name' => 'Centreon Entreprise Linux reposistory contains software to use with Centreon.',
        'path' => 'centreon-lts/1c0143e9c46ef424bc0c1ae9ba77203c17abf1cda7e'
    ),
    'bam' => array(
        'name' => 'Centreon Entreprise Linux reposistory contains software to use with Centreon.',
        'path' => 'centreon-bam/d4e1d7d3e888f596674453d1f20ff6d3'
    ),
    'map' => array(
        'name' => 'Centreon Entreprise Linux reposistory contains software to use with Centreon.',
        'path' => 'centreon-map/bfcfef6922ae08bd2b641324188d8a5f'
    ),
    'mbi' => array(
        'name' => 'Centreon Entreprise Linux reposistory contains software to use with Centreon.',
        'path' => 'centreon-mbi/5e0524c1c4773a938c44139ea9d8b4d7'
    ),
    'plugin-packs' => array(
        'name' => 'Centreon Entreprise Linux reposistory contains software to use with Centreon.',
        'path' => 'plugin-packs/2e83f5ff110c44a9cab8f8c7ebbe3c4f'
    )
);

// Generate all .repo files.
foreach ($repos as $repo => $repodata) {
    // Process CentOS 6 and CentOS 7.
    foreach (array('el6', 'el7') as $distrib) {
        @mkdir($distrib);
        $content = '';

        // Process all flavors.
        foreach (array('stable', 'testing', 'unstable') as $flavor) {
            // Process architectures.
            $archs = array('noarch', '$basearch');
            foreach ($archs as $arch) {
                // Header [centreon-map-stable].
                $content .= '[centreon';
                if (!empty($repo)) {
                    $content .= '-' . $repo;
                }
                $content .= '-' . $flavor;
                if ($arch == 'noarch') {
                    $content .= '-noarch';
                }
                $content .= "]\n";

                // Description.
                $content .= 'name=' . $repodata['name'] . "\n";
                $content .= 'baseurl=http://yum.centreon.com/' . $repodata['path'] . '/' . $centreonversion . '/' . $distrib . '/' . $flavor . '/' . $arch . '/' . "\n";
                $content .= 'enabled=' . ($flavor == 'stable' ? 1 : 0) . "\n";
                $content .= "gpgcheck=1\n";
                $content .= "gpgkey=file:///etc/pki/rpm-gpg/RPM-GPG-KEY-CES\n\n";
            }
        }

        // Write repo file.
        file_put_contents(
            $distrib . '/centreon' . (empty($repo) ? '' : '-' . $repo) . '.repo',
            $content
        );
    }
}

// Generate all spec files.
foreach ($repos as $repo => $repodata) {
    // Generate spec content.
    $content = '';
    $content =
        'Name:      centreon' . (empty($repo) ? '' : '-' . $repo) . '-release' . "\n" .
        'Version:   ' . $centreonversion . "\n" .
        'Release:   ' . $reporelease . '%{?dist}' . "\n" .
        'Summary:   ' . $repodata['name'] . "\n" .
        'Group:     Applications/Communications' . "\n" .
        'License:   ' . (empty($repo) ? 'ASL 2.0' : 'Proprietary') . "\n" .
        'URL:       https://www.centreon.com' . "\n" .
        'Packager:  Matthieu Kermagoret <mkermagoret@centreon.com>' . "\n" .
        'Vendor:    Centreon' . "\n" .
        'BuildArch: noarch' . "\n" .
        'Source0:   centreon' . (empty($repo) ? '' : '-' . $repo) . '.repo' . "\n";
    if (empty($repo)) {
        $content .=
            'Provides:  ces-release' . "\n" .
            'Obsoletes: ces-release' . "\n" .
            'Source1:   RPM-GPG-KEY-CES' . "\n";
    } else {
        $content .=
            'Requires: centreon-release' . "\n";
    }
    $content .=
        'BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root' . "\n" .
        'Requires:  yum' . "\n" .
        "\n" .
        '%description' . "\n" .
        'Official repository of Centreon.' . "\n" .
        "\n" .
        '%install' . "\n" .
        '%{__install} -d $RPM_BUILD_ROOT%{_sysconfdir}/yum.repos.d' . "\n" .
        '%{__cp} %SOURCE0 $RPM_BUILD_ROOT%{_sysconfdir}/yum.repos.d/' . "\n";
    if (empty($repo)) {
        $content .=
            '%{__install} -d $RPM_BUILD_ROOT%{_sysconfdir}/pki/rpm-gpg' . "\n" .
            '%{__cp} %SOURCE1 $RPM_BUILD_ROOT%{_sysconfdir}/pki/rpm-gpg/' . "\n";
    }
    $content .=
        "\n" .
        '%clean' . "\n" .
        '%{__rm} -rf $RPM_BUILD_ROOT' . "\n" .
        "\n" .
        '%files' . "\n" .
        '%defattr(-,root,root,-)' . "\n" .
        '%{_sysconfdir}/yum.repos.d/centreon' . (empty($repo) ? '' : '-' . $repo) . '.repo' . "\n";
    if (empty($repo)) {
        $content .=
            '%{_sysconfdir}/pki/rpm-gpg/RPM-GPG-KEY-CES' . "\n";
    }
    $content .=
        "\n" .
        '%changelog' . "\n";

    // Write spec file.
    file_put_contents(
        'centreon' . (empty($repo) ? '' : '-' . $repo) . '.spec',
        $content
    );
}

// Write EMS spec file.
foreach ($repos as $repo => $repodata) {
    $content =
        'Name:      centreon-ems-release' . "\n" .
        'Version:   ' . $centreonversion . "\n" .
        'Release:   ' . $reporelease . '%{?dist}' . "\n" .
        'Summary:   Install all Centreon repositories.' . "\n" .
        'Group:     Applications/Communications' . "\n" .
        'License:   Proprietary' . "\n" .
        'URL:       https://www.centreon.com' . "\n" .
        'Packager:  Matthieu Kermagoret <mkermagoret@centreon.com>' . "\n" .
        'Vendor:    Centreon' . "\n" .
        'BuildArch: noarch' . "\n" .
        'Requires:  centreon-release' . "\n" .
        'Requires:  centreon-bam-release' . "\n" .
        'Requires:  centreon-map-release' . "\n" .
        'Requires:  centreon-mbi-release' . "\n" .
        'Requires:  centreon-plugin-packs-release' . "\n";
    file_put_contents(
        'centreon-ems.spec',
        $content
    );
}
