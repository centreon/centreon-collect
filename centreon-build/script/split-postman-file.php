<?php

/* Validate the options */
$options = getopt('f:d:');

if (!is_file($options['f']) || !is_readable($options['f'])) {
    echo "The source file must be exits and be readable.\n";
    exit(1);
}

if (!is_dir($options['d']) || !is_writable($options['d'])) {
    echo "The destination directory must be exits and be writable.\n";
    exit(1);
}

/* Open the configuration file */
try {
    $fileContent = file_get_contents($options['f']);
    $restFile = json_decode($fileContent, true);
    unset($fileContent);
} catch (\Exception $err) {
    echo $err;
    exit(1);
}

$listGroups = array(
    /* Configuraytion */
    '00-Configure_Poller' => array(
        'name' => 'Configure Poller',
        'file' => 'config.poller'
    ),
    '01-Configure_Centreon_Engine' => array(
        'name' => 'Configure Centreon Engine',
        'file' => 'config.centreon-engine'
    ),
    '02-Configure_Centreon_Broker' => array(
        'name' => 'Configure Centreon Broker',
        'file' => 'config.centreon-broker',
    ),
    '10-Timeperiods' => array(
        'name' => 'Configure timeperiods',
        'file' => 'config.timeperiod'
    ),
    '11-Commands' => array(
        'name' => 'Configure commands',
        'file' => 'config.command'
    ),
    '20-Contacts templates' => array(
        'name' => 'Configure contact templates',
        'file' =>'config.contact-template'
    ),
    '21-Contacts' => array(
        'name' => 'Configure contracts',
        'file' => 'config.contact'
    ),
    '22-Contactgroups' => array(
        'name' => 'Configure contractgroups',
        'file' => 'config.contactgroup'
    ),
    '30-Hosts_Templates' => array(
        'name' => 'Configure host templates',
        'file' => 'config.host-template'
    ),
    '31-Hosts' => array(
        'name' => 'Configure hosts',
        'file' => 'config.host'
    ),
    '32-Hostgroups' => array(
        'name' => 'Configure hostrgoups',
        'file' => 'config.hostgroup'
    ),
    '33-Hosts_Categories' => array(
        'name' => 'Configure host catergories',
        'file' => 'config,host-category'
    ),
    '40-Services_Templates' => array(
        'name' => 'Configure service templates',
        'file' => 'config.service-template'
    ),
    '41-Services' => array(
        'name' => 'Configure service',
        'file' => 'config.service'
    ),
    '42-Service_by_Hostgroups' => array(
        'name' => 'Configure service by hostgroups',
        'file' => 'config.service-by-hostgroup'
    ),
    '43-Servicegroups' => array(
        'name' => 'Configure servicegroups',
        'file' => 'config.servicegroup'
    ),
    '44-Services_Categories' => array(
        'name' => 'Configure service categories',
        'file' => 'config.service-category'
    ),
    '50-Traps_Vendors' => array(
        'name' => 'Configure trap vendors',
        'file' => 'config.traps.vendors'
    ),
    '51-Traps_SNMP' => array(
        'name' => 'Configure traps',
        'file' => 'config.traps.snmp'
    ),
    '60-Downtimes' => array(
        'name' => 'Configure downtimes',
        'file' => 'config.downtimes'
    ),
    '61-Dependencies' => array(
        'name' => 'Configure dependencies',
        'file' => 'config.dependencies'
    ),
    '70-ACL_Groups' => array(
        'name' => 'Configure ACL groups',
        'file' => 'admin.acl.group'
    ),
    '71-ACL_Menus' => array(
        'name' => 'Configure ACL menu',
        'file' => 'admin.acl.menu'
    ),
    '72-ACL_Resources' => array(
        'name' => 'Configure ACL resources',
        'file' => 'admin.acl.resources'
    ),
    '74-ACL_Actions' => array(
        'name' => 'Configure ACL actions',
        'file' => 'admin.acl.actions'
    ),
    '75-ACL_Reload' => array(
        'name' => 'Configure ACL reload',
        'file' => 'admin.acl.reload'
    ),
    '80-Administration_General_Settings' => array(
        'name' => 'Manage general settings',
        'file' => 'admin.general'
    ),
    '81-Administration_LDAP_Settings' => array(
        'name' => 'Manage LDAP',
        'file' => 'admin.ldap'
    ),
    '90-Engine_CFG' => null,
    '99-Delete' => null,
    /* Realtime */
    '10-Host' => array(
        'name' => 'Realtime host',
        'file' => 'rt.host'
    ),
    '20-Service' => array(
        'name' => 'Realtime service',
        'file' => 'rt.service'
    ),
    '50-Downtime Realtime' => array(
        'name' => 'Realtime downtime',
        'file' => 'rt.downtime'
    )
);

foreach ($restFile['item'] as $group) {
    if (isset($listGroups[$group['name']]) && !is_null($listGroups[$group['name']])) {
        echo "Process " . $listGroups[$group['name']]['name'] . "\n";
        $jsonOutput = array(
            'info' => array(
                'name' => $listGroups[$group['name']]['name'],
                '_postman_id' => uniqid(),
                'description' => $listGroups[$group['name']]['name'],
                'schema' => 'https://schema.getpostman.com/json/collection/v2.1.0/collection.json'
            ),
            'item' => $group['item']
        );
        file_put_contents(
            $options['d'] . '/centreon.api.' . $listGroups[$group['name']]['file'] . '.postman_collection.json',
            json_encode($jsonOutput)
        );
        unset($jsonOutput);
    }
}

echo "Finished\n";
