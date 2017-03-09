-- Clean old Javascript files declared for page "Performances" in 'Monitoring > Business Activity > Performances'
DELETE FROM `topology_JS` WHERE id_page = 20709 AND PathName_js = './include/common/javascript/codebase/dhtmlxtree.js';
DELETE FROM `topology_JS` WHERE id_page = 20709 AND PathName_js = './include/common/javascript/codebase/dhtmlxcommon.js';