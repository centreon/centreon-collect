<?php
/*
 * Centreon
 *
 * Source Copyright 2005-2016 Centreon
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more informations : contact@centreon.com
 *
 */

class BaContactgroup extends Contactgroup {
    protected $generate_filename = 'centreon-bam-contactgroups.cfg';

    public function generateObjects() {
        if ($this->checkGenerate(0)) {
            return 0;
        }

        Engine::getInstance()->addCfgPath($this->generate_filename);
        Engine::getInstance()->addCfgPath(BaContact::getInstance()->getGenerateFile());

        $ba_contactgroups = BaService::getInstance()->getContactgoups();

        foreach ($ba_contactgroups as $cg_id => $cg_name) {
            $this->getContactFromCgId($cg_id);
            $this->generateFromCgId($cg_id, $cg_name);
        }
    }

    public function generateFromCgId($cg_id, $cg_name) {
        if (is_null($cg_id)) {
            return null;
        }

        $this->buildCache();
        if ($this->use_cache == 1) {
            if (!isset($this->cg_cache[$cg_id])) {
                return null;
            }
            $this->cg[$cg_id] = &$this->cg_cache[$cg_id];
        } else if (!isset($this->cg[$cg_id])) {
            $this->getCgFromId($cg_id);
        }

        if (is_null($this->cg[$cg_id])) {
            return null;
        }
        if ($this->checkGenerate($cg_id)) {
            return $this->cg[$cg_id]['contactgroup_name'];
        }

        $this->getContactFromCgId($cg_id);

        $this->cg[$cg_id]['contactgroup_name'] = $cg_name;

        $this->generateObjectInFile($this->cg[$cg_id], $cg_id);
    }

    public function getContactFromCgId($cg_id) {
        if (!isset($this->cg[$cg_id]['members_cache'])) {
            if (is_null($this->stmt_contact)) {
                $this->stmt_contact = $this->backend_instance->db->prepare("SELECT
                        contact_contact_id
                    FROM contactgroup_contact_relation
                    WHERE contactgroup_cg_id = :cg_id
                ");
            }
            $this->stmt_contact->bindParam(':cg_id', $cg_id, PDO::PARAM_INT);
            $this->stmt_contact->execute();
            $this->cg[$cg_id]['members_cache'] = $this->stmt_contact->fetchAll(PDO::FETCH_COLUMN);
        }

        $ba_contact_instance = BaContact::getInstance();
        $this->cg[$cg_id]['members'] = array();
        foreach ($this->cg[$cg_id]['members_cache'] as $contact_id) {
            $member = $ba_contact_instance->generateFromContactId($contact_id);
            # Can have contact template in a contact group ???!!
            if (!is_null($member) && !$ba_contact_instance->isTemplate($contact_id)) {
                $this->cg[$cg_id]['members'][] = $member;
            }
        }
    }
}
