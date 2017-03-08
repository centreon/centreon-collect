<script type="text/javascript" src="./include/common/javascript/changetab.js"></script>
<script type="text/javascript" src="./include/common/javascript/jquery/plugins/jquery.nicescroll.min.js"></script>
<script type="text/javascript" src="./modules/centreon-export/core/js/discoveryCommandValidation.js"></script>
<link rel="stylesheet" href="./modules/centreon-export/core/css/styles.css">
{$form.javascript}{$javascript}
<form {$form.attributes}>

    <div class="headerTabContainer">
        <ul id="mainnav">
            <li class="a" id='c1'><a href="#" class="tablink" style='cursor:pointer' onclick="javascript:montre('1');">{$tab1}</a></li>
            <li class="b" id='c2'><a href="#" class="tablink" style='cursor:pointer' onclick="javascript:montre('2');">{$tab2}</a></li>
            <li class="b" id='c3'><a href="#" class="tablink" style='cursor:pointer' onclick="javascript:montre('3');">{$tab3}</a></li>
        </ul>
        <div id="validFormTop">
        <p>{$form.submitA.html}</p>
        </div>
    </div>

    <div id='tab1' class='tab'>
        <table class="formTable table">
            <tr class="ListHeader">
                <td class="FormHeader">
                    <h3>| {$form.header.title}</h3>
                </td>
            </tr>
            <tr class="list_lvl_1">
                <td class="ListColLvl1_name" colspan="2">
                    <h4>{$basic_info_label}</h4>
                </td>
            </tr>
            <tr class="list_one">
                <td class="FormRowField">{$form.name.label}</td>
                <td class="FormRowValue">{$form.name.html}</td>
            </tr>
            <tr class="list_one">
                <td class="FormRowField">{$form.display_name.label}</td>
                <td class="FormRowValue">{$form.display_name.html}</td>
            </tr>
            <tr class="list_two">
                <td class="FormRowField">{$form.status.label}</td>
                <td class="FormRowValue">{$form.status.html}</td>
            </tr>
            <tr class="list_one">
                <td class="FormRowField">{$form.icon.label}</td>
                <td class="FormRowValue">
                    {if isset($image)}
                    <span class="preview_icon">
                        <img src="data:image/gif;base64,{$image}" name='icon_image' id='icon_image' />
                    </span> 
                    {/if}
                    <div class="field-group">
                    {$form.icon.html} (Max file size {$max_uploader_file}) {$form.clear.html}
                    </div>
                </td> 
            </tr>
            <tr class="list_two">
                <td class="FormRowField">{$form.tag.label}</td>
                <td class="FormRowValue">{$form.tag.html}</td>
            </tr>
            
            <tr class="list_lvl_1">
                <td class="ListColLvl1_name" colspan="2">
                    <h4>{$basic_credentials_label}</h4>
                </td>
            </tr>
            <tr class="list_one">
                <td class="FormRowField">{$form.author.label}</td>
                <td class="FormRowValue">{$form.author.html}</td>
            </tr>
            <tr class="list_two">
                <td class="FormRowField">{$form.email.label}</td>
                <td class="FormRowValue">{$form.email.html}</td>
            </tr>
            <tr class="list_one">
                <td class="FormRowField">{$form.website.label}</td>
                <td class="FormRowValue">{$form.website.html}</td>
            </tr>
            
            <tr class="list_lvl_1">
                <td class="ListColLvl1_name" colspan="2">
                    <h4>{$basic_discovery_label}</h4>
                </td>
            </tr>
            <tr class="list_one">
                <td class="FormRowField">{$form.command.label}</td>
                <td class="FormRowValue">{$form.command.html}</td>
            </tr>
            <tr class="list_two">
                <td class="FormRowField">{$form.type_discovery.label}</td>
                <td class="FormRowValue">{$form.type_discovery.html}</td>
            </tr>
            
            <tr class="list_lvl_1">
                <td class="ListColLvl1_name" colspan="2">
                    <h4>{$basic_requirement_label}</h4>
                </td>
            </tr>
            <tr class="list_one">
                <td class="FormRowField">{$requirement_label}</td>
                <td class="FormRowValue">{include file="file:$dir_tpl/clone.ihtml" cloneId="requirement" cloneSet=$cloneRequirement}</td>
            </tr>
            <tr class="list_two">
                <td class="FormRowField">{$dependency_label}</td>
                <td class="FormRowValue">{include file="file:$dir_tpl/clone.ihtml" cloneId="dependency" cloneSet=$cloneDependency}</td>
            </tr>
            <tr class="list_one">
                <td class="FormRowField">{$form.compatibility.label}</td>
                <td class="FormRowValue">{$form.compatibility.html}</td>
            </tr>
        </table>
    </div>
    <div id='tab2' class='tab'>
        <table class="formTable table">
            <tr class="ListHeader">
                <td class="FormHeader">
                    <h3>| {$form.header.title}</h3>
                </td>
            </tr>
            <tr class="list_lvl_1">
                <td class="ListColLvl1_name" colspan="2">
                    <h4>{$form.header.inclusion}</h4>
                </td>
            </tr>
            <tr class="list_one">
                <td class="FormRowValue" colspan="2">{$form.hostIncluded.label}</td>
            </tr>
            <tr>
                <td class="FormRowValue" width="50%">
                    <div class='ppm_filter'><input type='text' id='filterHostIncluded' placeholder="{$search}" /></div>
                    <div id="htTplIncluded" class="elemBloc htTpl ppe_tpl">
                        {$form.hostIncluded.html}
                    </div>
                </td>
                    <td>
                        
                        <div class="elemBloc advancedForm">
                            <table class="formTable table">
                                <tr class="list_one">
                                    <td class="FormRowField">{$form.htpl_discovery_protocol.label}</td>
                                    <td class="FormRowValue">{$form.htpl_discovery_protocol.html}</td>
                                </tr>
                                <tr class="list_two">
                                    <td class="FormRowField">{$form.htpl_discovery_category.label}</td>
                                    <td class="FormRowValue">{$form.htpl_discovery_category.html}</td>
                                </tr>
                                <tr class="list_one">
                                    <td class="FormRowField">{$form.htpl_discovery_command.label}</td>
                                    <td class="FormRowValue">{$form.htpl_discovery_command.html}</td>
                                </tr>
                                <tr class="list_two">
                                    <td class="FormRowField">{$form.htpl_documentation.label}</td>
                                    <td class="FormRowValue">{$form.htpl_documentation.html}</td>
                                </tr>
                                <tr>
                                    <td colspan="2">
                                        <p style="text-align: center;padding:8px;"><a class="btc bt_success" href="#" id="submitAdForm" >Done</a></p>
                                    </td>
                                </tr>
                            </table>
                        </div>
                    </td>
            </tr>
            <tr class="list_two">
                <td class="FormRowValue" colspan="2">
                    {$form.serviceIncluded.label}
                </td>
            </tr>
            <tr>
                <td class="FormRowValue" width="50%">
                    <div class='ppm_filter'><input type='text' id='filterServiceIncluded' placeholder="{$search}" /></div>
                    <div id="serviceTplIncluded" class="elemBloc serviceTpl ppe_tpl">
                        {$form.serviceIncluded.html}
                    </div>
                </td>
                    <td>
                        <div class="elemBloc advancedFormService">
                            <table class="formTable table">
                                <tr class="list_one">
                                    <td class="FormRowField">{$form.stpl_discovery_command.label}</td>
                                    <td class="FormRowValue">{$form.stpl_discovery_command.html}</td>
                                </tr>
                                <tr>
                                    <td colspan="2">
                                        <p style="text-align: center;padding:8px;"><a class="btc bt_success" href="#" id="submitAdFormSvc" >Done</a></p>
                                    </td>
                                </tr>
                            </table>
                        </div>
                    </td>
            </tr>

            <tr class="list_lvl_1">
                <td class="ListColLvl1_name" colspan="2">
                    <h4>{$form.header.exclusion}</h4>
                </td>
            </tr>
            <tr class="list_one">
                <td class="FormRowValue">{$form.hostExcluded.label}</td>
                <td class="FormRowValue">{$form.serviceExcluded.label}</td>
            </tr>
            <tr>
                <td class="FormRowValue">
                    <div class='ppm_filter'><input type='text' id='filterHostExcluded' placeholder="{$search}" /></div>
                    <div id="htTplExcluded" class="elemBloc ppe_tpl">
                        {$form.hostExcluded.html}
                    </div>
                </td>
                <td class="FormRowValue">
                    <div class='ppm_filter'><input type='text' id='filterServiceExcluded' placeholder="{$search}" /></div>
                    <div id="stTplExcluded" class="elemBloc ppe_tpl">
                        {$form.serviceExcluded.html}
                    </div>
                </td>
            </tr>
            <tr class="list_lvl_2"><td class="ListColLvl2_name" colspan="2">{$form.required._note}</td></tr>
        </table>
    </div>
    <div id='tab3' class='tab'>
        <table class="formTable table">
            <tr class="ListHeader">
                <td class="FormHeader" colspan="2">
                    <h3>| {$form.header.title}</h3>
                </td>
            </tr>
            <tr class="list_lvl_1">
                <td class="ListColLvl1_name" colspan="2">
                    <h4>{$descriptionLabel}</h4>
                </td>
            </tr>
            <tr class="list_two">
                <td class="FormRowValue" colspan="2">
                    <div id="tabs" class="vertical-tabs">
                        <ul>
                            {foreach from=$localeList key="key" item="item"}
                                <li class="ico-tab">
                                    <a href="#{$item.locale_short_name}-tab">
                                        <img src="./img/icons/flag/{$item.locale_img}" alt="{$item.locale_img}" />
                                    </a>
                               </li>
                           {/foreach}
                       </ul>
                       
                       {foreach from=$localeList key="key" item="item"}
                            <div id="{$item.locale_short_name}-tab" class="lang-wrapper">
                                <span class="lang-abr">{$item.locale_short_name}</span>
                                {assign var="textareaName" value=$item.locale_short_name}
                                {$form.description.$textareaName.html}
                            </div>
                       {/foreach}
                    </div>
                </td>
            </tr>
            <tr class="list_one">
                <td class="FormRowField">{$form.monitoring_procedure.label}</td>
                <td class="FormRowValue">{$form.monitoring_procedure.html}</td>
            </tr>
        </table>
    </div>
    <div id="validForm">
         <p id="validForm">{$form.submitA.html}</p>
    </div>
        {$form.hidden}
</form>
{literal}
<script type='text/javascript'>
jQuery.expr[':'].contains = function(a, i, m) {
    return jQuery(a).text().toUpperCase().indexOf(m[3].toUpperCase()) >= 0;
};

jQuery(function() {
    jQuery("#clear").click(function () {
        jQuery("input[name='icon']").val("");
        jQuery("input[name='id_icon']").val("");
        jQuery("#icon_image").remove();
    });
    
    /* Vertical tabs in Documentation tab*/
    
    jQuery( "#tabs" ).tabs().addClass( "ui-tabs-vertical ui-helper-clearfix" );
    jQuery( "#tabs li" ).removeClass( "ui-corner-top" ).addClass( "ui-corner-left" );
    
    /* --------------------------- */
    
    jQuery('input[name="icon"]').change(function() {
        jQuery("input[name='id_icon']").val("");
        jQuery("#icon_image").remove();
    });
       
    jQuery('select[name^=nameDependency]').trigger('change');
    
   jQuery("#filterHostExcluded, #filterHostIncluded, #filterServiceExcluded, #filterServiceIncluded").live("keyup",function(e) {
        var id = jQuery(this).attr('id');
        var includeOrExclude = id.substr(id.length - 8);
        var hostOrService = (id.substr(6, id.indexOf(includeOrExclude) - 6)).toLowerCase();
        includeOrExclude = includeOrExclude.toLowerCase();
        jQuery("." + includeOrExclude +"." + hostOrService).parent().show();
        var valeur = this.value;
        if (jQuery.trim(valeur) !== '') {
            jQuery("." + includeOrExclude + "." + hostOrService).not(jQuery("label:contains('" + valeur + "')").siblings("." + includeOrExclude + "." + hostOrService)).parent().hide();
        }
        if ( e.which === 13 ) return false;
        if ( e.which === 13 ) e.preventDefault();
    });
});

function setListener(elem){
    elem.on('change',function(event, data){
        if(typeof data != "undefined" && typeof data.origin !=undefined 
            && data.origin == "select2defaultinit"){
            return false;
        }
        doAjaxLoad(this);
    });

    elem.on("remove", function () {
        jQuery(this).val("");
        doAjaxLoad(this);
    });
}

function clonerefreshListener(el){
    setListener(el.find('select[name^=nameDependency]'));
}

function doAjaxLoad(elem)
{
    if (jQuery(elem).length > 0) {
        
        var aVersionElement = jQuery('#clone-values-dependency').data('clone-values-dependency');
        var aVersion = [];
        jQuery.each(aVersionElement, function(index, el) {
           aVersion[index]=  el['versionDependency_#index#'];
        });
    
        var iid = elem.id;
        var i = iid.substring(iid.lastIndexOf('_') + 1);
        
        var _version = jQuery(elem).next();
        if (_version.length > 0) {
            jQuery(_version).find('option').remove();
            jQuery.ajax({
                url: "./api/internal.php?object=centreon_export_plugin_pack&action=versions&slug=" + jQuery(elem).val(),
                type: "GET",
                dataType : "json",
                success : function(json){
                  jQuery.each(json.versions, function(key, value) {
                      jQuery(_version)
                          .append(jQuery("<option></option>")
                          .attr("value",value)
                          .text(value));
                          jQuery(_version).val(aVersion[i]);
                  });
                }
            });
        }
    }
}

jQuery(".elemBloc").niceScroll({cursorcolor:"#bcbdc0"});

jQuery(".tablink").on("click", function(){
    jQuery(".elemBloc").getNiceScroll().resize();
});

jQuery(document).ready(function() {
    jQuery('input[type="checkbox"]').each(function(){
        var $checkbox = jQuery(this);
        var elemID = $checkbox.attr("id");
        manageCheckbox(elemID, jQuery(this).prop("checked"));

        $checkbox.change(function() {
            manageCheckbox(elemID, jQuery(this).prop("checked"));
        });
    });

    function manageCheckbox(id, checked) {
        var splittedId = id.split('_');
        if (splittedId.length != 3) {
            return;
        }

        var objectType = splittedId[1];
        var objectId = splittedId[2];
        var objectAction = 'included';
        if (splittedId[0] == 'included') {
            objectAction = 'excluded';
        }
        

        var oppositeId = objectAction + '_' + objectType + '_' + objectId;       
        if (checked) {
            jQuery('#' + oppositeId).parent().closest('div').hide();
        } else {
            jQuery('#' + oppositeId).parent().closest('div').show();
        }
        jQuery(".elemBloc").getNiceScroll().resize();

    }

    var parent = jQuery(".htTpl");
    var form = jQuery(".advancedForm");
    var checkbox = jQuery(".htTpl input:checkbox"); 
    var inputHidden = jQuery("#elem_id");
    var span = jQuery('.checkbox-tpl > span');
    
    var pr = jQuery("input[name='htpl_discovery_protocol']");
    var ca = jQuery("select[name='htpl_discovery_category']");
    var co = jQuery("input[name='htpl_discovery_command']");
    var doc = jQuery("input[name='htpl_documentation']");
    
    checkbox.each(function(){
        var $checkbox = jQuery(this);
        var elemID = $checkbox.attr("id");
        var span = jQuery("#span_" + elemID);

        $checkbox.change(function() {
            // Checkbox is activated 
            courant = span.parent();
            parent = span.parent().parent();
            if(jQuery(this).prop("checked") === true) {
                // Afficher le formulaire et activer le bouton edition
                span.removeClass("disabled").addClass("enabled");
                jQuery(courant).detach();
                jQuery(parent).prepend(courant);
                
                form.show();
                inputHidden.val(elemID);
            } else  {
                IdOriginalPlace = parseInt(span.next().data('place'));
                if (IdOriginalPlace > 0) {
                    originalPlace = parent.find("[data-place='" + (IdOriginalPlace - 1) + "']").parent();
                    jQuery(courant).detach();
                    jQuery(courant).insertAfter(originalPlace);
                }
                form.hide(); 
                span.removeClass("enabled").addClass("disabled");
            }
        });
    });

    jQuery("#submitAdForm").on('click',function() {
        co.parent().find('#discovery_command_message').remove();
        if (discoveryCommandValidation(co.val())) {
            updateHostForm();
        } else {
            co.parent().prepend('<div id="discovery_command_message"><i style="color:red;">{/literal}{$discovery_command_message}{literal}</i><br></div>');
        }
    });

    function updateHostForm() {
        var hpr = jQuery("input[id='protocol_" + inputHidden.val() + "']");
        var hca = jQuery("input[id='discovery_category_" + inputHidden.val() + "']");
        var hco = jQuery("input[id='discovery_command_" + inputHidden.val() + "']");
        var hdoc = jQuery("input[id='documentation_" + inputHidden.val() + "']");

        hpr.val(pr.val());
        hca.val(ca.val());
        hco.val(co.val());
        hdoc.val(doc.val());

        form.hide();

        inputHidden.val("");
        pr.val("");
        ca.val("");
        co.val("");
        doc.val("");
    }

    span.each(function() {
        var $span = jQuery(this);
        var elemID = $span.attr("id").substring(5);
        var parentCheckbox = jQuery("#" + elemID);

        if (parentCheckbox.prop("checked") === true) {
            $span.removeClass("disabled").addClass("enabled"); 
        }
        jQuery(this).on('click', function(){
            if(jQuery(this).hasClass('enabled')) {
                form.show();
                inputHidden.val(elemID);
                               
                var hpr = jQuery("input[id='protocol_" + inputHidden.val() + "']");
                var hca = jQuery("input[id='discovery_category_" + inputHidden.val() + "']");
                var hco = jQuery("input[id='discovery_command_" + inputHidden.val() + "']");
                var hdoc = jQuery("input[id='documentation_" + inputHidden.val() + "']");

                pr.val(hpr.val());
                ca.val(hca.val());
                co.val(hco.val()); 
                doc.val(hdoc.val());
                
            } else {
                form.hide();
            }
        });
    });
    
    
    //Manage service template
    var parentSvc = jQuery(".serviceTpl");
    var formSvc = jQuery(".advancedFormService");
    var checkboxSvc = jQuery(".serviceTpl input:checkbox");
    var inputHiddenSvc = jQuery("#svc_elem_id");
    var spanSvc = jQuery('.checkbox-servicetpl > span');
    var coSvc = jQuery("input[name='stpl_discovery_command']");
    
    checkboxSvc.each(function(){
        var $checkboxSvc = jQuery(this);
        var elemIDSvc = $checkboxSvc.attr("id");
        var spanSvc = jQuery("#spanService_" + elemIDSvc);

        $checkboxSvc.change(function() {
            //Checkbox is activated
            courant = spanSvc.parent();
            parent = spanSvc.parent().parent();
            if(jQuery(this).prop("checked") === true) {
                //Afficher le formulaire et activer le bouton edition
                spanSvc.removeClass("disabled").addClass("enabled");
                formSvc.show();
                inputHiddenSvc.val(elemIDSvc);
                jQuery(courant).detach();
                jQuery(parent).prepend(courant);    
            } else  {
                IdOriginalPlace = parseInt(spanSvc.next().data('place'));
                if (IdOriginalPlace > 0) {
                    originalPlace = parent.find("[data-place='" + (IdOriginalPlace - 1) + "']").parent();
                    jQuery(courant).detach();
                    jQuery(courant).insertAfter(originalPlace);
                }
                
                formSvc.hide(); 
                spanSvc.removeClass("enabled").addClass("disabled");
            }
        });
    });

    jQuery("#submitAdFormSvc").on('click',function() {
        var hcoSvc = jQuery("input[id='discovery_command_" + inputHiddenSvc.val() + "']");
        hcoSvc.val(coSvc.val());
        formSvc.hide();
        inputHiddenSvc.val("");
        coSvc.val("");
     });

    spanSvc.each(function() {
        var $spanSvc = jQuery(this);
        var elemIDSvc = $spanSvc.attr("id").substring(12);
        var parentCheckboxSvc = jQuery("#" + elemIDSvc);

        if (parentCheckboxSvc.prop("checked") === true) {
            $spanSvc.removeClass("disabled").addClass("enabled"); 
        }
        jQuery(this).on('click', function(){
            if(jQuery(this).hasClass('enabled')) {
                formSvc.show();
                inputHiddenSvc.val(elemIDSvc);
                var hcoSvc = jQuery("input[id='discovery_command_" + inputHiddenSvc.val() + "']");
                coSvc.val(hcoSvc.val());
                
                console.log($spanSvc);
                console.log(parentCheckboxSvc);
                courant = $spanSvc.parent();
                parent = $spanSvc.parent().parent();
                jQuery(courant).detach();
                jQuery(parent).prepend(courant);
                
            } else {
                console.log($spanSvc);
                console.log(parentCheckboxSvc);
                formSvc.hide();
            }
        });
    });
});  

</script>
{/literal}
