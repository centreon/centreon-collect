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
    . 'release_date, status, nb_ht, nb_st, nb_c, install_count, '
    . 'released, category, requirement, changelog, information) '
    . 'VALUES (:pluginpack_id, :version, :release_date, :status, '
    . ':nb_ht, :nb_st, :nb_c, :install_count, :released, :category, '
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
    . 'VALUES ((SELECT id FROM catalog WHERE catalog_level = :catalog_level), :pluginpack_id)'
);

// Get PP list.
$base_dir = '/usr/share/centreon-packs';
chdir($base_dir);

$catalogs = scandir('./');
foreach ($catalogs as $catalog) {
    if (!is_dir($catalog)) {
        continue;
    }

    $ppList = glob($catalog . '/*.json');
    foreach ($ppList as $ppFile) {
        // Load Plugin Pack.
        $ppJson = file_get_contents($ppFile);
        $ppContent = json_decode($ppJson, TRUE);
        $ppVersion = $ppContent['information']['version'];

        // INSERT INTO pluginpack.
        $ppinsert->bindParam(':name', $ppContent['information']['name']);
        $ppinsert->bindParam(':slug', $ppContent['information']['slug']);
        $community = 0;
        $ppinsert->bindParam(':community', $community);
        $certified = 0;
        $ppinsert->bindParam(':certified', $certified);
        $ppinsert->bindParam(':icon', $ppContent['information']['icon']);
        $ppinsert->execute();
        $res = $dbh->query(
            "SELECT id FROM pluginpack WHERE slug='"
            . $ppContent['information']['slug'] . "'"
        );
        $row = $res->fetch();
        $ppId = $row['id'];
        unset($res);

        // INSERT INTO pluginpack_version.
        $ppvinsert->bindParam(':pluginpack_id', $ppId);
        $ppvinsert->bindParam(':version', $ppVersion);
        $releaseDate = date('Y-m-d H:i:s', $ppContent['information']['update_date']);
        $ppvinsert->bindParam(':release_date', $releaseDate);
        $ppvinsert->bindParam(':status', $ppContent['information']['status']);
        $htCount = count($ppContent['host_templates']);
        $ppvinsert->bindParam(':nb_ht', $htCount);
        $stCount = count($ppContent['service_templates']);
        $ppvinsert->bindParam(':nb_st', $stCount);
        $cmdCount = count($ppContent['commands']);
        $ppvinsert->bindParam(':nb_c', $cmdCount);
        $installCount = 0;
        $ppvinsert->bindParam(':install_count', $installCount);
        $released = true;
        $ppvinsert->bindParam(':released', $released);
        if (!empty($ppContent['information']['discovery_category'])) {
            $ppvinsert->bindParam(':category', $ppContent['information']['discovery_category']);
        }
        else {
            $category = 'unknown';
            $ppvinsert->bindParam(':category', $category);
        }
        if (!empty($ppContent['information']['requirement'])) {
            $dummy = '[' . json_encode($ppContent['information']['requirement'][0]);
            foreach (array_slice($ppContent['information']['requirement'], 1) as $requirement) {
                $dummy .= ', ' . json_encode($requirement);
            }
            $dummy .= ']';
            $ppvinsert->bindParam(':requirement', $dummy);
        }
        else {
            $dummy = null;
            $ppvinsert->bindParam(':requirement', $dummy, \PDO::PARAM_NULL);
        }
        $ppvinsert->bindParam(':changelog', $ppContent['information']['changelog']);
        $ppvinsert->bindParam(':information', $ppJson);
        $ppvinsert->execute();

        // INSERT INTO pluginpack_description.
        foreach ($ppContent['information']['description'] as $description) {
            $ppdinsert->bindParam(':pluginpack_id', $ppId);
            $ppdinsert->bindParam(':version', $ppVersion);
            $ppdinsert->bindParam(':locale', $description['lang']);
            $ppdinsert->bindParam(':description', $description['value']);
            $ppdinsert->execute();
        }

        // INSERT INTO pluginpack_tag.
        foreach ($ppContent['information']['tags'] as $tag) {
            $pptinsert->bindParam(':pluginpack_id', $ppId);
            $pptinsert->bindParam(':version', $ppVersion);
            $pptinsert->bindParam(':tag', $tag);
            $pptinsert->execute();
        }

        // INSERT INTO catalog_pluginpack.
        if (preg_match('/^catalog\-(\d)$/', $catalog, $matches)) {
            $cppinsert->bindParam(':catalog_level', $matches[1]);
            $cppinsert->bindParam(':pluginpack_id', $ppId);
            $cppinsert->execute();
        }
    }
}
