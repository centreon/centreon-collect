<link rel="stylesheet" href="./Themes/Centreon-2/markdown.css">
<script type="text/javascript" src="./include/common/javascript/tool.js"></script>
<script type="text/javascript" src="./include/common/javascript/marked.js"></script>
<script type="text/javascript" src="./modules/centreon-export/core/js/FileSaver.min.js"></script>
<script type="text/javascript" src="./modules/centreon-export/core/js/jszip.min.js"></script>
<script type="text/javascript" src="./modules/centreon-export/core/js/export.js"></script>

<form name='form' method='POST'>
    <table class="ajaxOption table">
        <tbody>
            <tr>
                <th  colspan="4"><h5>{t}Filters{/t}</h5></th>
            </tr>
            <tr>
                <td><h4>{$form.searchP.label}</h4></td>
                <td><h4>{$form.version.label}</h4></td>
                <td><h4>{$form.modified.label}</h4></td>
                <td><h4>{$form.dateLimit.label}</h4></td>
            </tr>
            <tr>
                <td>{$form.searchP.html}</td>
                <td>{$form.version.html}</td>
                <td>{$form.modified.html}</td>
                <td>{$form.operator.html} {$form.dateLimit.html}</td>
                <td><input type="submit" value="{t}Search{/t}" class="btc bt_success"></td>
            </tr>
        </tbody>
    </table>
    <table class="ToolbarTable table">
        <tr class="ToolbarTR">
    <td>
              {$form.o1.html} <a href="{$msg.addL}" class="btc bt_success">{$msg.addT}</a>
              {$form.exportAll.html}
	    </td>
            {php}
                   include('./include/common/pagination.php');
            {/php}
        </tr>
    </table>
    <table  id="pluginPackTable" class="ListTable">
        <tr class="ListHeader">
            <td class="ListColHeaderPicker"><input type="checkbox" name="checkall" onclick="checkUncheckAll(this);"/></td>
            <td class="ListColHeaderCenter">{t}Name{/t}</td>
            <td class="ListColHeaderCenter">{t}Nb HTPL{/t}</td>	
            <td class="ListColHeaderCenter">{t}Nb STPL{/t}</td>	
            <td class="ListColHeaderCenter">{t}Nb commands{/t}</td>	
            <td class="ListColHeaderCenter">{t}Last version{/t}</td>
            <td class="ListColHeaderCenter">{t}Last Update{/t}</td>
            <td class="ListColHeaderCenter">{t}Date of the latest version{/t}</td>
            <td class="ListColHeaderCenter">{t}Actions{/t}</td>
        </tr>
        {foreach from=$myArray key=k item=v}
            {if $k is odd}
            <tr class="list_one">
            {else}
            <tr class="list_two">
            {/if}
                <td class="ListColPicker">
                  <input type="checkbox" name="select[{$v.plugin_id}]" value="1">
                </td>
                <td class="ListColLeft"><a href='?p=61101&o=e&id={$v.plugin_id}'>{$v.plugin_name}</a></td>
                <td class="ListColCenter">{$v.nbHostTpl}</td>
                <td class="ListColCenter">{$v.nbServiceTpl}</td>
                <td class="ListColCenter">{$v.nbCommand}</td>
                <td class="ListColCenter">{$v.plugin_version}</td>
                <td class="ListColCenter">{$v.last_update}</td>
                <td class="ListColCenter">{$v.time_generate}</td>
                <td class="ListColCenter">
                  <a href="#" class="pp-export" data-ppid="{$v.plugin_id}" data-ppname="{$v.plugin_name}">
                    <img src="./img/icons/download_file.png" class="ico-14">
                  </a>
                </td>
            </tr>
        {/foreach}
    </table>
    <table class="ToolbarTable table">
        <tr class="ToolbarTR">
            <td>
              {$form.o2.html} <a href="{$msg.addL}" class="btc bt_success">{$msg.addT}</a>
	        </td>
            {php}
                   include('./include/common/pagination.php');
            {/php}
        </tr>
    </table>
    <input type='hidden' name='o' id='o'>
    <input type='hidden' id='limit' name='limit' value='{$limit}'>
    {$form.hidden}
</form>
<!-- Popin for export all plugin packs -->
<div id="export-all-plugin-pack">
    <form novalidate>
        <table class="formTable table">
            <tbody>
                <tr class="ListHeader">
                    <td class="FormHeader">
                        <h3>{t}Export all plugin packs{/t}</h3>
                    </td>
                    <td>
                        <span class="msg pp-field-error pp-general-error" style="display: none;"></span>
                    </td>
                </tr>
                <tr class="list_lvl_1 viewerInfo">
                    <td class="ListColLvl1_name" colspan="2">
                        <h4>{t}Version{/t}</h4>
                    </td>
                </tr>
                <tr class="list_one">
                    <td class="FormRowField">{t}Increment version{/t}</td>
                    <td class="FormRowValue"><input id="increment_version" name="increment_version" type="checkbox"></td>
                </tr>
                <tr class="list_lvl_1 viewerInfo toHide">
                    <td class="ListColLvl1_name">
                        <h4>{t}Changelog{/t}</h4>
                    </td>
                    <td class="ListColLvl1_name">
                        <span class="msg pp-field-error" style="display: none"></span>
                    </td>
                </tr>
                <tr class="list_one toHide">
                    <td class="FormRowValue">
                        <h4>{t}Changelog{/t} <i style="color: red;" size="1">*</i></h4>
                    </td>
                    <td class="FormRowValue">
                        <h4>{t}Preview{/t}</h4>
                    </td>
                <tr class="list_one toHide">
                    <td class="FormRowValue">
                        <span class="msg pp-field-error pp-general-error" style="display: none;"></span>
                        <textarea name="changelog" class="default-textarea"></textarea>
                    </td>
                    <td class="FormRowValue">
                        <div class="md-preview"></div>
                    </td>
                </tr>
            </tbody>
        </table>
        <div class="button_group_center">
            <button type="button" class="btc bt_success">{t}Export{/t}</button>
            <button type="button" class="btc bt_default">{t}Cancel{/t}</button>
        </div>
    </form>
</div>
<!-- Popin for export an plugin pack -->
<div id="export-plugin-pack">
  <form novalidate>
    <table class="formTable table">
      <tbody>
        <tr class="ListHeader">
          <td class="FormHeader">
              <h3>{t}Export a plugin pack{/t} | <b><span class="pp-name"></span></b></h3>
          </td>
          <td>
            <span class="msg pp-field-error pp-general-error" style="display: none;"></span>
          </td>
        </tr>
        <tr class="list_lvl_1 viewerInfo">
          <td class="ListColLvl1_name" colspan="2">
            <h4>{t}Version{/t}</h4>
          </td>
        </tr>
        <tr class="list_one">
          <td class="FormRowField">{t}Version{/t} <i style="color: red;" size="1">*</i></td>
          <td class="FormRowValue">
            <input type="text" name="version" required size="12">
            <span class="msg pp-field-error" style="display: none"></span>
          </td>
        </tr>
        <tr class="list_lvl_1 viewerInfo">
          <td class="ListColLvl1_name" colspan="2">
            <h4>{t}Status{/t}</h4>
          </td>
        </tr>
        <tr class="list_one">
          <td class="FormRowField">{t}Status{/t} <i style="color: red;" size="1">*</i></td>
          <td class="FormRowValue">
            <select name="status" required>
              <option value="stable">{t}Stable{/t}</option>
              <option value="testing">{t}Testing{/t}</option>
              <option value="dev">{t}Development{/t}</option>
              <option value="experimental">{t}Experimental{/t}</option>
              <option value="deprecated">{t}Deprecated{/t}</option>
            </select>
          </td>
        </tr>
        <tr class="list_two">
          <td class="FormRowField">{t}Status message{/t}</td>
          <td class="FormRowValue">
            <input type="text" name="status_message" size="35">
          </td>
        </tr>
        <tr class="list_lvl_1 viewerInfo">
          <td class="ListColLvl1_name">
            <h4>{t}Changelog{/t}</h4>
          </td>
          <td class="ListColLvl1_name">
            <span class="msg pp-field-error" style="display: none"></span>
          </td>
        </tr>
        <tr class="list_one">
          <td class="FormRowValue">
            <h4>{t}Changelog{/t} <i style="color: red;" size="1">*</i></h4>
          </td>
          <td class="FormRowValue">
              <h4>{t}Preview{/t}</h4>
          </td>
        <tr class="list_one">
          <td class="FormRowValue">
            <span class="msg pp-field-error pp-general-error" style="display: none;"></span>
            <textarea name="changelog" class="default-textarea" required></textarea>
          </td>
          <td class="FormRowValue">
            <div class="md-preview"></div>
          </td>
        </tr>
          
        </tr>
      </tbody>
    </table>
    <input type="hidden" name="pluginpack_id">
    <div class="button_group_center">
      <button type="button" class="btc bt_success">{t}Export{/t}</button>
      <button type="button" class="btc bt_default">{t}Cancel{/t}</button>
    </div>
  </form>
</div>
