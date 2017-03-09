DELETE FROM topology where topology_page = '20709';
DELETE FROM topology_JS WHERE id_page = '20709';

UPDATE @DB_CENTSTORAGE@.metrics m, @DB_CENTSTORAGE@.index_data i
SET m.to_delete=1
WHERE m.index_id = i.id
AND i.service_description LIKE 'ba_%'
AND m.metric_name IN ('BA_Acknowledgement', 'BA_Downtime');

UPDATE giv_components_template SET ds_color_line = '#597f00', ds_color_area = '#88b917' WHERE name = 'BA_Level' AND ds_color_line = '#13F00E' AND ds_color_area = '#6DEC45';

DELETE FROM giv_components_template where name = 'BA_Downtime';
DELETE FROM giv_components_template where name = 'BA_Acknowledgement';