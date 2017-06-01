DELETE FROM options where `key` IN (
  'kb_db_name',
  'kb_db_host',
  'kb_db_user',
  'kb_db_password',
  'kb_db_prefix',
  'kb_wiki_url',
  'kb_wiki_account',
  'kb_wiki_password');
INSERT INTO options (`key`, `value`) VALUES
  ('kb_db_name', 'wikidb'),
  ('kb_db_host', 'mediawiki'),
  ('kb_db_user', 'centreon'),
  ('kb_db_password', 'centreon'),
  ('kb_db_prefix', ''),
  ('kb_wiki_url', 'http://mediawiki'),
  ('kb_wiki_account', 'WikiSysop'),
  ('kb_wiki_password', 'centreon');
