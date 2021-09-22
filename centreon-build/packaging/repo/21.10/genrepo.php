<?php

// Base centreon version.
$centreonversion = '21.10';
$reporelease = '1';

// Information table.
$repos = [
    '' => [
        'name' => 'Centreon open source software repository.',
        'path' => 'standard',
    ],
    'business' => [
        'name' => 'Centreon Business repository',
        'path' => 'centreon-business/1a97ff9985262bf3daf7a0919f9c59a6',
    ],
    'plugin-packs' => [
        'name' => 'Centreon Plugin Packs repository',
        'path' => 'plugin-packs/2e83f5ff110c44a9cab8f8c7ebbe3c4f',
    ],
];

// Generate all .repo files.
foreach ($repos as $repo => $repodata) {
    // Process CentOS 7 only.
    foreach (['el7', 'el8'] as $distrib) {
        @mkdir($distrib);
        $content = '';

        // Process all flavors.
        foreach (['stable', 'testing', 'unstable', 'canary'] as $flavor) {
            // Process architectures.
            $archs = ['noarch', '$basearch'];
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
                $content .= 'name=' . $repodata['name'] . ($flavor == 'stable' ? '' : ' (UNSUPPORTED)') . "\n";
                $content .= 'baseurl=https://yum.centreon.com/' . $repodata['path'] . '/' . $centreonversion . '/'
                    . $distrib . '/' . $flavor . '/' . $arch . '/' . "\n";
                $content .= 'enabled=' . ($flavor == 'stable' ? 1 : 0) . "\n";
                $content .= "gpgcheck=1\n";
                $content .= "gpgkey=file:///etc/pki/rpm-gpg/RPM-GPG-KEY-CES\n";
                if ($distrib == 'el8') {
                    $content .= "module_hotfixes=1\n";
                }
                $content .= "\n";
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
    $content =
        'Name:      centreon' . (empty($repo) ? '' : '-' . $repo) . '-release' . "\n" .
        'Version:   ' . $centreonversion . "\n" .
        'Release:   ' . $reporelease . '%{?dist}' . "\n" .
        'Summary:   ' . $repodata['name'] . "\n" .
        'Group:     Applications/Communications' . "\n" .
        'License:   ' . (empty($repo) ? 'ASL 2.0' : 'Proprietary') . "\n" .
        'URL:       https://www.centreon.com' . "\n" .
        'Packager:  Centreon Team <centreon@centreon.com>' . "\n" .
        'Vendor:    Centreon' . "\n" .
        'BuildArch: noarch' . "\n" .
        'Source0:   centreon' . (empty($repo) ? '' : '-' . $repo) . '.repo' . "\n";
    if (empty($repo)) {
        $content .=
            'Provides:  ces-release' . "\n" .
            'Obsoletes: ces-release' . "\n" .
            'Source1:   RPM-GPG-KEY-CES' . "\n";
    } elseif ($repo === 'business') {
        $content .=
            'Requires: centreon-release' . "\n" .
            'Obsoletes: centreon-bam-release' . "\n" .
            'Obsoletes: centreon-map-release' . "\n" .
            'Obsoletes: centreon-mbi-release' . "\n" .
            'Obsoletes: centreon-failover-release' . "\n";
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
        'Packager:  Centreon Team <centreon@centreon.com>' . "\n" .
        'Vendor:    Centreon' . "\n" .
        'BuildArch: noarch' . "\n" .
        'Requires:  centreon-release' . "\n" .
        'Requires:  centreon-business-release' . "\n";
    file_put_contents(
        'centreon-ems.spec',
        $content
    );
}
