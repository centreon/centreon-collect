<?php

// Prepare queries.
$dbh = new \PDO('mysql:host=localhost;dbname=imp', 'root', 'centreon');
$dbh->setAttribute(\PDO::ATTR_ERRMODE, \PDO::ERRMODE_EXCEPTION);
$ppinsert = $dbh->prepare(
    'INSERT IGNORE INTO pluginpack (name, slug, community, certified, icon) '
    . 'VALUES (:name, :slug, :community, :certified, :icon)'
);
$ppvinsert = $dbh->prepare(
    'INSERT INTO pluginpack_version (pluginpack_id, version, '
    . 'release_date, status, nb_ht, nb_st, nb_c, download_count, '
    . 'released, category, requirement, changelog, information) '
    . 'VALUES (:pluginpack_id, :version, :release_date, :status, '
    . ':nb_ht, :nb_st, :nb_c, :download_count, :released, :category, '
    . ':requirement, :changelog, :information)'
);
$ppdinsert = $dbh->prepare(
    'INSERT INTO pluginpack_description (pluginpack_id, version, '
    . 'locale, description) '
    . 'VALUES (:pluginpack_id, :version, :locale, :description)'
);
$pptinsert = $dbh->prepare(
    'INSERT INTO pluginpack_tag (pluginpack_id, version, tag) '
    . 'VALUES (:pluginpack_id, :version, :tag)'
);
$cppinsert = $dbh->prepare(
    'INSERT IGNORE INTO catalog_pluginpack (catalog_id, pluginpack_id) '
    . 'VALUES (:catalog_id, :pluginpack_id)'
);

// Get PP list.
$base_dir = '/usr/share/centreon-packs';
chdir($base_dir);
$pp_list = glob('*.json');

// Insert all Plugin Packs in DB.
foreach ($pp_list as $pp_file) {
    // Load Plugin Pack.
    $ppjson = file_get_contents($pp_file);
    $ppcontent = json_decode($ppjson, TRUE);
    $ppversion = $ppcontent['information']['version'];

    // INSERT INTO pluginpack.
    $ppinsert->bindParam(':name', $ppcontent['information']['name']);
    $ppinsert->bindParam(':slug', $ppcontent['information']['slug']);
    $community = 0;
    $ppinsert->bindParam(':community', $community);
    $certified = 0;
    $ppinsert->bindParam(':certified', $certified);
    $ppinsert->bindParam(':icon', $ppcontent['information']['icon']);
    $ppinsert->execute();
    $res = $dbh->query(
        "SELECT id FROM pluginpack WHERE slug='"
        . $ppcontent['information']['slug'] . "'");
    $row = $res->fetch();
    $ppid = $row['id'];
    unset($res);

    // INSERT INTO pluginpack_version.
    $ppvinsert->bindParam(':pluginpack_id', $ppid);
    $ppvinsert->bindParam(':version', $ppversion);
    $release_date = date('Y-m-d H:i:s', $ppcontent['information']['update_date']);
    $ppvinsert->bindParam(':release_date', $release_date);
    $ppvinsert->bindParam(':status', $ppcontent['information']['status']);
    $ht_count = count($ppcontent['host_templates']);
    $ppvinsert->bindParam(':nb_ht', $ht_count);
    $st_count = count($ppcontent['service_templates']);
    $ppvinsert->bindParam(':nb_st', $st_count);
    $cmd_count = count($ppcontent['commands']);
    $ppvinsert->bindParam(':nb_c', $cmd_count);
    $download_count = 0;
    $ppvinsert->bindParam(':download_count', $download_count);
    $released = true;
    $ppvinsert->bindParam(':released', $released);
    if (!empty($ppcontent['information']['discovery_category'])) {
        $ppvinsert->bindParam(':category', $ppcontent['information']['discovery_category']);
    }
    else {
        $category = 'unknown';
        $ppvinsert->bindParam(':category', $category);
    }
    if (!empty($ppcontent['information']['requirement'])) {
        $dummy = '[' . json_encode($ppcontent['information']['requirement'][0]);
        foreach (array_slice($ppcontent['information']['requirement'], 1) as $requirement) {
            $dummy .= ', ' . json_encode($requirement);
        }
        $dummy .= ']';
        $ppvinsert->bindParam(':requirement', $dummy);
    }
    else {
        $dummy = null;
        $ppvinsert->bindParam(':requirement', $dummy, \PDO::PARAM_NULL);
    }
    $ppvinsert->bindParam(':changelog', $ppcontent['information']['changelog']);
    $ppvinsert->bindParam(':information', $ppjson);
    $ppvinsert->execute();

    // INSERT INTO pluginpack_description.
    foreach ($ppcontent['information']['description'] as $description) {
        $ppdinsert->bindParam(':pluginpack_id', $ppid);
        $ppdinsert->bindParam(':version', $ppversion);
        $ppdinsert->bindParam(':locale', $description['lang']);
        $ppdinsert->bindParam(':description', $description['value']);
        $ppdinsert->execute();
    }

    // INSERT INTO pluginpack_tag.
    foreach ($ppcontent['information']['tags'] as $tag) {
        $pptinsert->bindParam(':pluginpack_id', $ppid);
        $pptinsert->bindParam(':version', $ppversion);
        $pptinsert->bindParam(':tag', $tag);
        $pptinsert->execute();
    }

    // INSERT INTO catalog_pluginpack.
    $free_id = 1;
    $registred_id = 2;
    $subscription_id = 3;
    switch ($ppcontent['information']['slug']) {
    case 'applications-databases-mysql':
    case 'applications-monitoring-centreon-central':
    case 'applications-monitoring-centreon-database':
    case 'applications-monitoring-centreon-poller':
    case 'base-generic':
    case 'hardware-printers-standard-rfc3805-snmp':
    case 'hardware-ups-standard-rfc1628-snmp':
    case 'network-cisco-standard-snmp':
    case 'operatingsystems-linux-snmp':
    case 'fake-unmanaged-objects':
    case 'fake-managed-objects':
    case 'fake-child-objects':
    case 'fake-wrong-discovery-category':
    case 'fake-timeperiods':
        $catalog_id = $free_id;
        break ;
    case 'applications-protocol-bgp':
    case 'applications-protocol-dns':
    case 'applications-protocol-http':
    case 'applications-protocol-imap':
    case 'applications-protocol-ldap':
    case 'applications-protocol-ntp':
    case 'applications-protocol-smtp':
    case 'applications-protocol-x509':
        $catalog_id = $registred_id;
        break ;
    default:
        $catalog_id = $subscription_id;
    }
    $cppinsert->bindParam(':catalog_id', $catalog_id);
    $cppinsert->bindParam(':pluginpack_id', $ppid);
    $cppinsert->execute();
}

?>
