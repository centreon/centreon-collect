-- Add topology
INSERT INTO topology (topology_name, topology_parent, topology_page, topology_order, topology_group, topology_url, topology_popup, topology_modules, topology_show)
VALUES ('Subscription', 507, 50707, 40, 1, './modules/centreon-license-manager/frontend/app/index.php', '0', '1', '1');
-- Insert empty companyToken
INSERT INTO options (`key`, `value`) VALUES ('impCompanyToken', '');