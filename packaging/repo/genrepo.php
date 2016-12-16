<?php

// Information table.
$repos = array(
    '' => array(
        'name' => 'Centreon Entreprise Linux reposistory contains software to use with Centreon.',
        'path' => 'standard'
    ),
    'bam' => array(
        'name' => 'Centreon Entreprise Linux reposistory contains software to use with Centreon.',
        'path' => 'bam/d4e1d7d3e888f596674453d1f20ff6d3'
    ),
    'map' => array(
        'name' => 'Centreon Entreprise Linux reposistory contains software to use with Centreon.',
        'path' => 'map/bfcfef6922ae08bd2b641324188d8a5f'
    ),
    'mbi' => array(
        'name' => 'Centreon Entreprise Linux reposistory contains software to use with Centreon.',
        'path' => 'mbi/5e0524c1c4773a938c44139ea9d8b4d7'
    ),
    'packs' => array(
        'name' => 'Centreon Entreprise Linux reposistory contains software to use with Centreon.',
        'path' => 'packs/2e83f5ff110c44a9cab8f8c7ebbe3c4f'
    )
);

// Browse all repositories.
foreach ($repos as $repo => $repodata) {
    // Process CentOS 6 and CentOS 7.
    foreach (array('el6', 'el7') as $distrib) {
        @mkdir($distrib);
        $content = '';

        // Process all flavors.
        foreach (array('stable', 'testing', 'unstable') as $flavor) {
            // Process architectures.
            $archs = empty($repo) ? array('noarch', '$basearch') : array('noarch');
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
                $content .= 'baseurl=http://yum.centreon.com/' . $repodata['path'] . '/3.4/' . $distrib . '/' . $flavor . '/' . $arch . '/' . "\n";
                $content .= 'enabled=' . ($flavor == 'stable' ? 1 : 0) . "\n";
                $content .= "gpgcheck=1\n";
                $content .= "gpgkey=file:///etc/pki/rpm-gpg/RPM-GPG-KEY-CES\n\n";
            }
        }

        // Write file.
        file_put_contents(
            $distrib . '/centreon' . (empty($repo) ? '' : '-' . $repo) . '.repo',
            $content
        );
    }
}
